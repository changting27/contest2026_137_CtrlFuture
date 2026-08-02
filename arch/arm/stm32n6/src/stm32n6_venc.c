/****************************************************************************
 * arch/arm/src/stm32n6/stm32n6_venc.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to you under the Apache License, Version
 * 2.0 (the "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.  See the License for the specific language governing
 * permissions and limitations under the License.
 *
 * STM32N6 VENC (H.264/JPEG hardware encoder) driver.
 *
 * This is a V4L2 memory-to-memory codec backend.  It plugs into the
 * NuttX video framework (drivers/video/v4l2_m2m.c): the framework owns
 * /dev/videoN and every VIDIOC_* ioctl, while this backend supplies the
 * codec_ops_s vtable.  Following the V4L2 M2M encoder convention the
 * OUTPUT queue carries raw YUV frames and the CAPTURE queue carries the
 * encoded H.264 bitstream.
 *
 * The STM32N6 VENC is a licensed Hantro H1 encoder.  The actual encode
 * step is driven by ST's H264 encoder library (h264encapi) against the
 * Hantro software registers -- that path is marked TODO(vendor) here.
 * The V4L2 plumbing (format negotiation, buffer sizing, stream control)
 * is implemented so the device node behaves correctly and the encode
 * core can be dropped in later.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/kmalloc.h>
#include <nuttx/wqueue.h>
#include <nuttx/video/v4l2_m2m.h>

#include <sys/videoio.h>
#include <stdint.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>

#include "stm32n6_venc.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define STM32N6_VENC_NAME     "stm32n6-venc"

/* Default frame geometry until VIDIOC_S_FMT overrides it. */

#define STM32N6_VENC_DEF_W    1280
#define STM32N6_VENC_DEF_H    720

/* Worst-case H.264 bitstream buffer: one uncompressed I-frame never
 * exceeds the raw YUV420 frame size (w*h*3/2), so use that as a safe
 * upper bound for the CAPTURE buffer.
 */

#define STM32N6_VENC_STRMSIZE(w, h)  (((w) * (h) * 3) / 2)

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct stm32n6_venc_s
{
  struct v4l2_format output_fmt;   /* Raw YUV input format */
  struct v4l2_format capture_fmt;  /* Encoded H.264 output format */
  struct work_s      work;         /* Deferred encode work */
  FAR void          *cookie;       /* Framework cookie for buffer I/O */
  bool               capture_on;   /* CAPTURE queue streaming */
  bool               output_on;    /* OUTPUT queue streaming */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int stm32n6_venc_open(FAR void *cookie, FAR void **priv);
static int stm32n6_venc_close(FAR void *priv);
static int stm32n6_venc_capture_streamon(FAR void *priv);
static int stm32n6_venc_output_streamon(FAR void *priv);
static int stm32n6_venc_capture_streamoff(FAR void *priv);
static int stm32n6_venc_output_streamoff(FAR void *priv);
static int stm32n6_venc_capture_available(FAR void *priv);
static int stm32n6_venc_output_available(FAR void *priv);
static int stm32n6_venc_querycap(FAR void *priv,
                                 FAR struct v4l2_capability *cap);
static int stm32n6_venc_capture_enum_fmt(FAR void *priv,
                                         FAR struct v4l2_fmtdesc *fmt);
static int stm32n6_venc_output_enum_fmt(FAR void *priv,
                                        FAR struct v4l2_fmtdesc *fmt);
static int stm32n6_venc_capture_g_fmt(FAR void *priv,
                                      FAR struct v4l2_format *fmt);
static int stm32n6_venc_output_g_fmt(FAR void *priv,
                                     FAR struct v4l2_format *fmt);
static int stm32n6_venc_capture_s_fmt(FAR void *priv,
                                      FAR struct v4l2_format *fmt);
static int stm32n6_venc_output_s_fmt(FAR void *priv,
                                     FAR struct v4l2_format *fmt);
static int stm32n6_venc_capture_try_fmt(FAR void *priv,
                                        FAR struct v4l2_format *fmt);
static int stm32n6_venc_output_try_fmt(FAR void *priv,
                                       FAR struct v4l2_format *fmt);
static size_t stm32n6_venc_capture_g_bufsize(FAR void *priv);
static size_t stm32n6_venc_output_g_bufsize(FAR void *priv);
static void stm32n6_venc_work(FAR void *arg);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct codec_ops_s g_stm32n6_venc_ops =
{
  .open              = stm32n6_venc_open,
  .close             = stm32n6_venc_close,
  .capture_streamon  = stm32n6_venc_capture_streamon,
  .output_streamon   = stm32n6_venc_output_streamon,
  .capture_streamoff = stm32n6_venc_capture_streamoff,
  .output_streamoff  = stm32n6_venc_output_streamoff,
  .capture_available = stm32n6_venc_capture_available,
  .output_available  = stm32n6_venc_output_available,
  .querycap          = stm32n6_venc_querycap,
  .capture_enum_fmt  = stm32n6_venc_capture_enum_fmt,
  .output_enum_fmt   = stm32n6_venc_output_enum_fmt,
  .capture_g_fmt     = stm32n6_venc_capture_g_fmt,
  .output_g_fmt      = stm32n6_venc_output_g_fmt,
  .capture_s_fmt     = stm32n6_venc_capture_s_fmt,
  .output_s_fmt      = stm32n6_venc_output_s_fmt,
  .capture_try_fmt   = stm32n6_venc_capture_try_fmt,
  .output_try_fmt    = stm32n6_venc_output_try_fmt,
  .capture_g_bufsize = stm32n6_venc_capture_g_bufsize,
  .output_g_bufsize  = stm32n6_venc_output_g_bufsize,
};

static struct codec_s g_stm32n6_venc_codec =
{
  .ops = &g_stm32n6_venc_ops,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: stm32n6_venc_work
 *
 * Description:
 *   Deferred encode step.  In a complete driver this dequeues a raw YUV
 *   buffer from the OUTPUT queue, runs the Hantro H1 encoder over it and
 *   enqueues the resulting H.264 access unit on the CAPTURE queue.
 *
 ****************************************************************************/

static void stm32n6_venc_work(FAR void *arg)
{
  FAR struct stm32n6_venc_s *venc = arg;

  UNUSED(venc);

  /* TODO(vendor): drive the Hantro H1 encoder.  Using ST's h264encapi:
   *   1. H264EncStrmStart() once per stream for SPS/PPS headers.
   *   2. Per frame: fetch the OUTPUT (YUV) buffer, set encIn.busLuma /
   *      busChromaU/V, pick intra vs. predicted, H264EncStrmEncode().
   *   3. Copy encOut into the CAPTURE buffer, mark bytesused, and hand
   *      both buffers back to the framework.
   * Requires the VENC clock (RCC APB5 VENCEN) and the VENCRAM AXI path
   * to be brought up first -- see RM0486 section 45 (VENC).
   */
}

/****************************************************************************
 * Name: stm32n6_venc_open / _close
 ****************************************************************************/

static int stm32n6_venc_open(FAR void *cookie, FAR void **priv)
{
  FAR struct stm32n6_venc_s *venc;

  venc = kmm_zalloc(sizeof(struct stm32n6_venc_s));
  if (venc == NULL)
    {
      return -ENOMEM;
    }

  venc->cookie = cookie;

  /* Seed sensible default formats: YUV420 in, H.264 out. */

  venc->output_fmt.fmt.pix.width        = STM32N6_VENC_DEF_W;
  venc->output_fmt.fmt.pix.height       = STM32N6_VENC_DEF_H;
  venc->output_fmt.fmt.pix.pixelformat  = V4L2_PIX_FMT_YUV420;
  venc->output_fmt.fmt.pix.field        = V4L2_FIELD_NONE;

  venc->capture_fmt.fmt.pix.width       = STM32N6_VENC_DEF_W;
  venc->capture_fmt.fmt.pix.height      = STM32N6_VENC_DEF_H;
  venc->capture_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_H264;
  venc->capture_fmt.fmt.pix.field       = V4L2_FIELD_NONE;
  venc->capture_fmt.fmt.pix.sizeimage   =
    STM32N6_VENC_STRMSIZE(STM32N6_VENC_DEF_W, STM32N6_VENC_DEF_H);

  *priv = venc;
  return OK;
}

static int stm32n6_venc_close(FAR void *priv)
{
  FAR struct stm32n6_venc_s *venc = priv;

  kmm_free(venc);
  return OK;
}

/****************************************************************************
 * Name: stream on/off handlers
 ****************************************************************************/

static int stm32n6_venc_capture_streamon(FAR void *priv)
{
  FAR struct stm32n6_venc_s *venc = priv;

  venc->capture_on = true;
  return OK;
}

static int stm32n6_venc_output_streamon(FAR void *priv)
{
  FAR struct stm32n6_venc_s *venc = priv;

  venc->output_on = true;
  return OK;
}

static int stm32n6_venc_capture_streamoff(FAR void *priv)
{
  FAR struct stm32n6_venc_s *venc = priv;

  venc->capture_on = false;
  return OK;
}

static int stm32n6_venc_output_streamoff(FAR void *priv)
{
  FAR struct stm32n6_venc_s *venc = priv;

  venc->output_on = false;
  return OK;
}

/****************************************************************************
 * Name: buffer-available handlers
 ****************************************************************************/

static int stm32n6_venc_capture_available(FAR void *priv)
{
  FAR struct stm32n6_venc_s *venc = priv;

  /* A CAPTURE (bitstream) buffer is ready to be filled.  Kick the encode
   * work item if both queues are streaming.
   */

  if (venc->capture_on && venc->output_on)
    {
      work_queue(HPWORK, &venc->work, stm32n6_venc_work, venc, 0);
    }

  return OK;
}

static int stm32n6_venc_output_available(FAR void *priv)
{
  FAR struct stm32n6_venc_s *venc = priv;

  /* A new raw YUV frame is queued on the OUTPUT side. */

  if (venc->capture_on && venc->output_on)
    {
      work_queue(HPWORK, &venc->work, stm32n6_venc_work, venc, 0);
    }

  return OK;
}

/****************************************************************************
 * Name: stm32n6_venc_querycap
 ****************************************************************************/

static int stm32n6_venc_querycap(FAR void *priv,
                                 FAR struct v4l2_capability *cap)
{
  UNUSED(priv);

  strlcpy((FAR char *)cap->driver, STM32N6_VENC_NAME,
          sizeof(cap->driver));
  strlcpy((FAR char *)cap->card, STM32N6_VENC_NAME, sizeof(cap->card));
  cap->capabilities = V4L2_CAP_VIDEO_M2M | V4L2_CAP_STREAMING;

  return OK;
}

/****************************************************************************
 * Name: format enumeration
 ****************************************************************************/

static int stm32n6_venc_capture_enum_fmt(FAR void *priv,
                                         FAR struct v4l2_fmtdesc *fmt)
{
  UNUSED(priv);

  if (fmt->index >= 1)
    {
      return -EINVAL;
    }

  fmt->pixelformat = V4L2_PIX_FMT_H264;
  return OK;
}

static int stm32n6_venc_output_enum_fmt(FAR void *priv,
                                        FAR struct v4l2_fmtdesc *fmt)
{
  UNUSED(priv);

  if (fmt->index >= 1)
    {
      return -EINVAL;
    }

  fmt->pixelformat = V4L2_PIX_FMT_YUV420;
  return OK;
}

/****************************************************************************
 * Name: get/set/try format
 ****************************************************************************/

static int stm32n6_venc_capture_g_fmt(FAR void *priv,
                                      FAR struct v4l2_format *fmt)
{
  FAR struct stm32n6_venc_s *venc = priv;

  *fmt = venc->capture_fmt;
  return OK;
}

static int stm32n6_venc_output_g_fmt(FAR void *priv,
                                     FAR struct v4l2_format *fmt)
{
  FAR struct stm32n6_venc_s *venc = priv;

  *fmt = venc->output_fmt;
  return OK;
}

static int stm32n6_venc_capture_s_fmt(FAR void *priv,
                                      FAR struct v4l2_format *fmt)
{
  FAR struct stm32n6_venc_s *venc = priv;

  /* CAPTURE side is always H.264; honour the requested geometry and
   * recompute the worst-case bitstream buffer size.
   */

  fmt->fmt.pix.pixelformat = V4L2_PIX_FMT_H264;
  fmt->fmt.pix.field       = V4L2_FIELD_NONE;
  fmt->fmt.pix.sizeimage   =
    STM32N6_VENC_STRMSIZE(fmt->fmt.pix.width, fmt->fmt.pix.height);

  venc->capture_fmt = *fmt;
  return OK;
}

static int stm32n6_venc_output_s_fmt(FAR void *priv,
                                     FAR struct v4l2_format *fmt)
{
  FAR struct stm32n6_venc_s *venc = priv;

  /* OUTPUT side is always YUV420. */

  fmt->fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
  fmt->fmt.pix.field       = V4L2_FIELD_NONE;

  venc->output_fmt = *fmt;
  return OK;
}

static int stm32n6_venc_capture_try_fmt(FAR void *priv,
                                        FAR struct v4l2_format *fmt)
{
  UNUSED(priv);

  fmt->fmt.pix.pixelformat = V4L2_PIX_FMT_H264;
  fmt->fmt.pix.field       = V4L2_FIELD_NONE;
  fmt->fmt.pix.sizeimage   =
    STM32N6_VENC_STRMSIZE(fmt->fmt.pix.width, fmt->fmt.pix.height);

  return OK;
}

static int stm32n6_venc_output_try_fmt(FAR void *priv,
                                       FAR struct v4l2_format *fmt)
{
  UNUSED(priv);

  fmt->fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
  fmt->fmt.pix.field       = V4L2_FIELD_NONE;

  return OK;
}

/****************************************************************************
 * Name: buffer sizing
 ****************************************************************************/

static size_t stm32n6_venc_capture_g_bufsize(FAR void *priv)
{
  FAR struct stm32n6_venc_s *venc = priv;

  return venc->capture_fmt.fmt.pix.sizeimage;
}

static size_t stm32n6_venc_output_g_bufsize(FAR void *priv)
{
  FAR struct stm32n6_venc_s *venc = priv;

  return STM32N6_VENC_STRMSIZE(venc->output_fmt.fmt.pix.width,
                               venc->output_fmt.fmt.pix.height);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: stm32n6_venc_register
 ****************************************************************************/

int stm32n6_venc_register(const char *devpath)
{
  int ret;

  ret = codec_register(devpath, &g_stm32n6_venc_codec);
  if (ret < 0)
    {
      syslog(LOG_ERR, "venc: codec_register(%s) failed: %d\n",
             devpath, ret);
      return ret;
    }

  syslog(LOG_INFO, "venc: registered H.264 M2M codec at %s\n", devpath);
  return OK;
}
