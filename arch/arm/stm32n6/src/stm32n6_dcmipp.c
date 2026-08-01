/****************************************************************************
 * arch/arm/src/stm32n6/stm32n6_dcmipp.c
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
 * STM32N6 DCMIPP (Digital Camera Interface Pixel Pipeline) driver.
 *
 * This is the V4L2 image data (imgdata) backend for the DCMIPP capture
 * engine.  It plugs into the NuttX video framework (drivers/video/
 * v4l2_cap.c): the framework owns /dev/videoN and every VIDIOC_* ioctl,
 * while this backend only programs the capture hardware and hands each
 * completed frame back through the stored capture callback.  A separate
 * image sensor backend (stm32n6_dcmipp_sensor.c) supplies the sensor
 * half that the framework binds against.
 *
 * The register-level pipe/ISP/DMA programming is marked TODO(RM): it
 * needs the STM32N6 reference-manual offsets (or the ST CMW_CAMERA
 * middleware), which are not yet available in-tree.  The control flow,
 * framework contract and buffer/callback bookkeeping are complete so the
 * pipeline can be exercised end-to-end once the register writes land.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/video/imgdata.h>
#include <nuttx/irq.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>

#include "stm32n6_dcmipp.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Hardware pipe indices.  The DCMIPP exposes independent pipes; EdgeSight
 * uses one for the display feed and one for the NN inference input.
 */

#define DCMIPP_PIPE_DISPLAY    0
#define DCMIPP_PIPE_NN         1
#define DCMIPP_PIPE_COUNT      2

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* imgdata backend instance.  struct imgdata_s must be the first member so
 * the framework's imgdata pointer can be cast to this type.
 */

struct stm32n6_dcmipp_data_s
{
  struct imgdata_s      data;         /* Base imgdata (must be first) */
  imgdata_capture_t     capture_cb;   /* Frame-complete callback */
  FAR void             *capture_arg;  /* Callback argument */
  FAR uint8_t          *buf_addr;     /* Current capture buffer */
  uint32_t              buf_size;     /* Capture buffer size in bytes */
  uint16_t              width;        /* Configured frame width */
  uint16_t              height;       /* Configured frame height */
  uint32_t              pixelformat;  /* IMGDATA_PIX_FMT_* */
  volatile bool         streaming;    /* Capture active */
  volatile uint32_t     frame_count;  /* Frames delivered */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int stm32n6_dcmipp_data_init(FAR struct imgdata_s *data);
static int stm32n6_dcmipp_data_uninit(FAR struct imgdata_s *data);
static int stm32n6_dcmipp_data_set_buf(FAR struct imgdata_s *data,
                                       uint8_t nr_datafmts,
                                       FAR imgdata_format_t *datafmts,
                                       FAR uint8_t *addr, uint32_t size);
static int stm32n6_dcmipp_data_validate_frame_setting(
                                    FAR struct imgdata_s *data,
                                    uint8_t nr_datafmts,
                                    FAR imgdata_format_t *datafmts,
                                    FAR imgdata_interval_t *interval);
static int stm32n6_dcmipp_data_start_capture(FAR struct imgdata_s *data,
                                    uint8_t nr_datafmts,
                                    FAR imgdata_format_t *datafmts,
                                    FAR imgdata_interval_t *interval,
                                    FAR imgdata_capture_t callback,
                                    FAR void *arg);
static int stm32n6_dcmipp_data_stop_capture(FAR struct imgdata_s *data);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct imgdata_ops_s g_stm32n6_dcmipp_ops =
{
  .init                   = stm32n6_dcmipp_data_init,
  .uninit                 = stm32n6_dcmipp_data_uninit,
  .set_buf                = stm32n6_dcmipp_data_set_buf,
  .validate_frame_setting = stm32n6_dcmipp_data_validate_frame_setting,
  .start_capture          = stm32n6_dcmipp_data_start_capture,
  .stop_capture           = stm32n6_dcmipp_data_stop_capture,
};

static struct stm32n6_dcmipp_data_s g_stm32n6_dcmipp =
{
  .data = { &g_stm32n6_dcmipp_ops },
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: stm32n6_dcmipp_fmt_supported
 *
 * Description:
 *   Return true when the pixel format can be produced by a DCMIPP pipe.
 *
 ****************************************************************************/

static bool stm32n6_dcmipp_fmt_supported(uint32_t pixelformat)
{
  switch (pixelformat)
    {
      case IMGDATA_PIX_FMT_RGB565:
      case IMGDATA_PIX_FMT_UYVY:
      case IMGDATA_PIX_FMT_YUYV:
        return true;
      default:
        return false;
    }
}

/****************************************************************************
 * Name: stm32n6_dcmipp_data_init
 *
 * Description:
 *   Bring the DCMIPP capture engine out of reset and enable its clocks.
 *
 ****************************************************************************/

static int stm32n6_dcmipp_data_init(FAR struct imgdata_s *data)
{
  FAR struct stm32n6_dcmipp_data_s *priv =
    (FAR struct stm32n6_dcmipp_data_s *)data;

  priv->streaming   = false;
  priv->frame_count = 0;
  priv->capture_cb  = NULL;
  priv->capture_arg = NULL;

  /* TODO(RM): enable DCMIPP clock in RCC, deassert reset, configure the
   * parallel/CSI input interface and the pipe muxing registers.
   */

  syslog(LOG_INFO, "dcmipp: imgdata backend initialized\n");
  return OK;
}

/****************************************************************************
 * Name: stm32n6_dcmipp_data_uninit
 *
 * Description:
 *   Stop any active capture and gate the DCMIPP clocks.
 *
 ****************************************************************************/

static int stm32n6_dcmipp_data_uninit(FAR struct imgdata_s *data)
{
  FAR struct stm32n6_dcmipp_data_s *priv =
    (FAR struct stm32n6_dcmipp_data_s *)data;

  priv->streaming = false;

  /* TODO(RM): disable pipe capture, mask interrupts, gate DCMIPP clock. */

  return OK;
}

/****************************************************************************
 * Name: stm32n6_dcmipp_data_set_buf
 *
 * Description:
 *   Program the destination address of the next capture DMA transfer.
 *   Called by the framework each time a buffer is queued.
 *
 ****************************************************************************/

static int stm32n6_dcmipp_data_set_buf(FAR struct imgdata_s *data,
                                       uint8_t nr_datafmts,
                                       FAR imgdata_format_t *datafmts,
                                       FAR uint8_t *addr, uint32_t size)
{
  FAR struct stm32n6_dcmipp_data_s *priv =
    (FAR struct stm32n6_dcmipp_data_s *)data;

  if (addr == NULL || size == 0)
    {
      return -EINVAL;
    }

  UNUSED(nr_datafmts);
  UNUSED(datafmts);

  priv->buf_addr = addr;
  priv->buf_size = size;

  /* TODO(RM): write addr to the pipe's DMA destination register
   * (DCMIPP_PxPPM0AR1) so the next frame lands in this buffer.
   */

  return OK;
}

/****************************************************************************
 * Name: stm32n6_dcmipp_data_validate_frame_setting
 *
 * Description:
 *   Report whether the requested frame geometry / format / rate can be
 *   produced.  Called before start_capture during VIDIOC_S_FMT handling.
 *
 ****************************************************************************/

static int stm32n6_dcmipp_data_validate_frame_setting(
                                    FAR struct imgdata_s *data,
                                    uint8_t nr_datafmts,
                                    FAR imgdata_format_t *datafmts,
                                    FAR imgdata_interval_t *interval)
{
  UNUSED(data);
  UNUSED(interval);

  if (nr_datafmts < 1 || datafmts == NULL)
    {
      return -EINVAL;
    }

  if (datafmts[IMGDATA_FMT_MAIN].width == 0 ||
      datafmts[IMGDATA_FMT_MAIN].height == 0)
    {
      return -EINVAL;
    }

  if (!stm32n6_dcmipp_fmt_supported(
        datafmts[IMGDATA_FMT_MAIN].pixelformat))
    {
      return -EINVAL;
    }

  return OK;
}

/****************************************************************************
 * Name: stm32n6_dcmipp_data_start_capture
 *
 * Description:
 *   Latch the frame settings and the framework capture callback, then
 *   start the pipe.  The callback is invoked from the frame interrupt
 *   (stm32n6_dcmipp_frame_event) once a transfer completes.
 *
 ****************************************************************************/

static int stm32n6_dcmipp_data_start_capture(FAR struct imgdata_s *data,
                                    uint8_t nr_datafmts,
                                    FAR imgdata_format_t *datafmts,
                                    FAR imgdata_interval_t *interval,
                                    FAR imgdata_capture_t callback,
                                    FAR void *arg)
{
  FAR struct stm32n6_dcmipp_data_s *priv =
    (FAR struct stm32n6_dcmipp_data_s *)data;

  if (nr_datafmts < 1 || datafmts == NULL)
    {
      return -EINVAL;
    }

  UNUSED(interval);

  priv->width       = datafmts[IMGDATA_FMT_MAIN].width;
  priv->height      = datafmts[IMGDATA_FMT_MAIN].height;
  priv->pixelformat = datafmts[IMGDATA_FMT_MAIN].pixelformat;
  priv->capture_cb  = callback;
  priv->capture_arg = arg;
  priv->streaming   = true;

  /* TODO(RM): configure pipe pixel format / crop / downsize registers
   * for width x height, enable the frame-complete interrupt and set the
   * pipe capture-enable bit (DCMIPP_PxFCTCR / DCMIPP_CMCR).
   */

  syslog(LOG_INFO, "dcmipp: capture start %ux%u fmt=%lu\n",
         priv->width, priv->height,
         (unsigned long)priv->pixelformat);
  return OK;
}

/****************************************************************************
 * Name: stm32n6_dcmipp_data_stop_capture
 *
 * Description:
 *   Stop the pipe and clear the capture callback.
 *
 ****************************************************************************/

static int stm32n6_dcmipp_data_stop_capture(FAR struct imgdata_s *data)
{
  FAR struct stm32n6_dcmipp_data_s *priv =
    (FAR struct stm32n6_dcmipp_data_s *)data;

  priv->streaming   = false;
  priv->capture_cb  = NULL;
  priv->capture_arg = NULL;

  /* TODO(RM): clear the pipe capture-enable bit and mask its interrupt. */

  syslog(LOG_INFO, "dcmipp: capture stopped\n");
  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: stm32n6_dcmipp_register
 ****************************************************************************/

int stm32n6_dcmipp_register(void)
{
  imgdata_register(&g_stm32n6_dcmipp.data);
  syslog(LOG_INFO, "dcmipp: registered imgdata backend\n");
  return OK;
}

/****************************************************************************
 * Name: stm32n6_dcmipp_frame_event
 ****************************************************************************/

void stm32n6_dcmipp_frame_event(uint32_t pipe)
{
  FAR struct stm32n6_dcmipp_data_s *priv = &g_stm32n6_dcmipp;
  imgdata_capture_t cb;
  FAR void *arg;

  if (pipe >= DCMIPP_PIPE_COUNT || !priv->streaming)
    {
      return;
    }

  priv->frame_count++;

  /* Hand the completed frame back to the video framework.  result 0
   * signals a good frame; buf_size is the number of valid bytes.
   */

  cb  = priv->capture_cb;
  arg = priv->capture_arg;

  if (cb != NULL)
    {
      cb(0, priv->buf_size, NULL, arg);
    }
}
