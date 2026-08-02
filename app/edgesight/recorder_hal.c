/****************************************************************************
 * app/edgesight/recorder_hal.c
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
 * EdgeSight - Recorder HAL implementation.
 *
 * Records event clips by feeding raw YUV frames to the STM32N6 H.264
 * hardware encoder and writing the encoded bitstream to the SD card.
 *
 * The encoder is reached through the V4L2 memory-to-memory codec node
 * that stm32n6_venc.c registers at /dev/video1 (OUTPUT queue = raw YUV,
 * CAPTURE queue = H.264 bitstream).  This HAL is the M2M client: it
 * negotiates formats through the codec node and owns the output file.
 * A missing node is non-fatal so the app still runs on hosts and on
 * boards where the encoder is not wired up.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "recorder_hal.h"

#include <sys/ioctl.h>
#include <sys/videoio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <syslog.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define RECORDER_HAL_DEVPATH "/dev/video1"

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: recorder_hal_resolution
 *
 * Description:
 *   Translate a REC_RES_* preset into pixel dimensions.
 *
 ****************************************************************************/

static void recorder_hal_resolution(uint8_t preset,
                                     uint32_t *width, uint32_t *height)
{
  switch (preset)
    {
      case REC_RES_1080P:
        *width = 1920;
        *height = 1080;
        break;

      case REC_RES_720P:
        *width = 1280;
        *height = 720;
        break;

      case REC_RES_480P:
      default:
        *width = 640;
        *height = 480;
        break;
    }
}

/****************************************************************************
 * Name: recorder_hal_negotiate
 *
 * Description:
 *   Set the OUTPUT (raw YUV420) and CAPTURE (H.264) formats on the codec
 *   node.  This drives the VENC codec_ops_s format handlers.
 *
 ****************************************************************************/

static int recorder_hal_negotiate(struct recorder_context_s *ctx)
{
  struct v4l2_format fmt;
  uint32_t width;
  uint32_t height;

  recorder_hal_resolution(ctx->config.resolution, &width, &height);

  /* OUTPUT queue: raw YUV420 frames fed to the encoder. */

  memset(&fmt, 0, sizeof(fmt));
  fmt.type                = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  fmt.fmt.pix.width       = width;
  fmt.fmt.pix.height      = height;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
  fmt.fmt.pix.field       = V4L2_FIELD_NONE;

  if (ioctl(ctx->codec_fd, VIDIOC_S_FMT, (unsigned long)&fmt) < 0)
    {
      syslog(LOG_ERR, "recorder: S_FMT(OUTPUT) failed: %d\n", errno);
      return -errno;
    }

  /* CAPTURE queue: encoded H.264 bitstream. */

  memset(&fmt, 0, sizeof(fmt));
  fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width       = width;
  fmt.fmt.pix.height      = height;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_H264;
  fmt.fmt.pix.field       = V4L2_FIELD_NONE;

  if (ioctl(ctx->codec_fd, VIDIOC_S_FMT, (unsigned long)&fmt) < 0)
    {
      syslog(LOG_ERR, "recorder: S_FMT(CAPTURE) failed: %d\n", errno);
      return -errno;
    }

  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int recorder_hal_init(struct recorder_context_s *ctx,
                      const struct recorder_config_s *cfg)
{
  memset(ctx, 0, sizeof(*ctx));
  ctx->config = *cfg;
  ctx->output_fd = -1;

  /* Open the V4L2 M2M encoder node.  A missing node is non-fatal: the
   * HAL stays usable on hosts and on boards without a wired encoder.
   */

  ctx->codec_fd = open(RECORDER_HAL_DEVPATH, O_RDWR);
  if (ctx->codec_fd < 0)
    {
      syslog(LOG_WARNING, "recorder: %s unavailable (%d), "
             "recording disabled\n", RECORDER_HAL_DEVPATH, errno);
    }
  else if (recorder_hal_negotiate(ctx) < 0)
    {
      close(ctx->codec_fd);
      ctx->codec_fd = -1;
    }

  ctx->state = REC_STATE_IDLE;
  ctx->initialized = true;

  syslog(LOG_INFO, "recorder: initialized %ukbps %ufps GOP=%u\n",
         (unsigned)cfg->bitrate_kbps,
         (unsigned)cfg->fps,
         (unsigned)cfg->gop_size);
  return 0;
}

int recorder_hal_start(struct recorder_context_s *ctx,
                       uint32_t event_id)
{
  enum v4l2_buf_type type;
  char path[64];

  if (!ctx->initialized || ctx->state == REC_STATE_RECORDING)
    {
      return -1;
    }

  /* Create the event clip file on the SD card. */

  snprintf(path, sizeof(path), "%s/event_%04u.h264",
           ctx->config.output_dir ? ctx->config.output_dir : "/mnt/sd",
           (unsigned)event_id);

  ctx->output_fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (ctx->output_fd < 0)
    {
      syslog(LOG_ERR, "recorder: cannot create %s: %d\n", path, errno);
      return -errno;
    }

  /* Start both M2M queues on the encoder node. */

  if (ctx->codec_fd >= 0)
    {
      type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
      if (ioctl(ctx->codec_fd, VIDIOC_STREAMON,
                (unsigned long)&type) < 0)
        {
          syslog(LOG_ERR, "recorder: STREAMON(OUTPUT) failed: %d\n",
                 errno);
        }

      type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      if (ioctl(ctx->codec_fd, VIDIOC_STREAMON,
                (unsigned long)&type) < 0)
        {
          syslog(LOG_ERR, "recorder: STREAMON(CAPTURE) failed: %d\n",
                 errno);
        }
    }

  ctx->state = REC_STATE_RECORDING;
  ctx->frame_count = 0;

  syslog(LOG_INFO, "recorder: recording event #%lu -> %s\n",
         (unsigned long)event_id, path);
  return 0;
}

int recorder_hal_feed_frame(struct recorder_context_s *ctx,
                            const void *frame, uint32_t size)
{
  if (!ctx->initialized || ctx->state != REC_STATE_RECORDING)
    {
      return -1;
    }

  UNUSED(frame);
  UNUSED(size);

  /* TODO(vendor): pump one frame through the M2M encoder.  With the
   * VENC encode core in place (stm32n6_venc.c, Hantro h264encapi) this
   * would:
   *   1. QBUF the raw YUV 'frame' on the OUTPUT queue.
   *   2. DQBUF the encoded access unit from the CAPTURE queue.
   *   3. write() the bitstream bytes to ctx->output_fd.
   *   4. re-QBUF the now-empty CAPTURE buffer.
   * The QBUF/DQBUF pump is intentionally not implemented yet: the codec
   * node's encode core is a vendor-library stub, so a blocking DQBUF
   * here would never complete.
   */

  ctx->frame_count++;

  /* Enforce the maximum clip duration. */

  if (ctx->config.max_duration_s > 0 &&
      ctx->frame_count >= ctx->config.fps * ctx->config.max_duration_s)
    {
      syslog(LOG_INFO, "recorder: max duration reached, stopping\n");
      recorder_hal_stop(ctx);
    }

  return 0;
}

int recorder_hal_stop(struct recorder_context_s *ctx)
{
  enum v4l2_buf_type type;

  if (!ctx->initialized || ctx->state != REC_STATE_RECORDING)
    {
      return -1;
    }

  /* Stop both encoder queues. */

  if (ctx->codec_fd >= 0)
    {
      type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
      ioctl(ctx->codec_fd, VIDIOC_STREAMOFF, (unsigned long)&type);
      type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      ioctl(ctx->codec_fd, VIDIOC_STREAMOFF, (unsigned long)&type);
    }

  /* Close the clip file. */

  if (ctx->output_fd >= 0)
    {
      close(ctx->output_fd);
      ctx->output_fd = -1;
    }

  ctx->state = REC_STATE_IDLE;

  syslog(LOG_INFO, "recorder: stopped, %lu frames\n",
         (unsigned long)ctx->frame_count);
  return 0;
}

void recorder_hal_get_stats(const struct recorder_context_s *ctx,
                            struct recorder_stats_s *stats)
{
  memset(stats, 0, sizeof(*stats));
  stats->state = ctx->state;
  stats->frames_encoded = ctx->frame_count;
}

void recorder_hal_deinit(struct recorder_context_s *ctx)
{
  if (!ctx->initialized)
    {
      return;
    }

  if (ctx->state == REC_STATE_RECORDING)
    {
      recorder_hal_stop(ctx);
    }

  if (ctx->codec_fd >= 0)
    {
      close(ctx->codec_fd);
      ctx->codec_fd = -1;
    }

  memset(ctx, 0, sizeof(*ctx));
  syslog(LOG_INFO, "recorder: deinitialized\n");
}
