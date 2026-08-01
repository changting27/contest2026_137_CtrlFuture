/****************************************************************************
 * app/edgesight/camera_hal.c
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
 * EdgeSight - Camera HAL implementation.
 * Wraps DCMIPP dual-pipeline + ISP middleware for NuttX.
 *
 * Integrates with arch/arm/stm32n6/stm32n6_dcmipp.c driver.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "camera_hal.h"
#include "memory_map.h"
#include <string.h>
#include <stdio.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/videoio.h>

/* Camera capture device node registered by the arch-level DCMIPP
 * imgdata backend + sensor.  The application talks to the driver
 * through the standard V4L2 interface only, never through chip
 * private headers.
 */

#define CAMERA_HAL_DEVPATH "/dev/video0"

/****************************************************************************
 * Private Data
 ****************************************************************************/

static camera_frame_cb_t g_callbacks[2];
static void *g_cb_args[2];

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int camera_hal_init(struct camera_context_s *ctx, uint32_t fps)
{
  memset(ctx, 0, sizeof(*ctx));

  ctx->sensor_width = 2592;   /* IMX335 default */
  ctx->sensor_height = 1944;
  ctx->fps = fps;

  /* Open the V4L2 capture device exposed by the DCMIPP driver.
   * A missing node is non-fatal: the HAL stays usable on hosts and
   * on boards where the camera pipeline is not wired yet.
   */

  ctx->fd = open(CAMERA_HAL_DEVPATH, O_RDWR);
  if (ctx->fd < 0)
    {
      syslog(LOG_WARNING, "camera: %s unavailable (%d), "
             "running headless\n", CAMERA_HAL_DEVPATH, errno);
    }

  ctx->initialized = true;

  syslog(LOG_INFO, "camera: initialized @ %lu fps\n",
         (unsigned long)fps);
  return 0;
}

/****************************************************************************
 * Name: camera_hal_v4l2_pixfmt
 *
 * Description:
 *   Map a CAM_FMT_xxx value to a V4L2 fourcc pixel format.
 *
 ****************************************************************************/

static uint32_t camera_hal_v4l2_pixfmt(uint8_t fmt)
{
  switch (fmt)
    {
      case CAM_FMT_RGB565:
        return V4L2_PIX_FMT_RGB565;
      case CAM_FMT_RGB888:
        return V4L2_PIX_FMT_RGB24;
      case CAM_FMT_YUV422:
        return V4L2_PIX_FMT_UYVY;
      case CAM_FMT_YUV420:
        return V4L2_PIX_FMT_YUV420;
      default:
        return V4L2_PIX_FMT_RGB565;
    }
}

/****************************************************************************
 * Name: camera_hal_set_format
 *
 * Description:
 *   Apply a pipe configuration through VIDIOC_S_FMT when a device is
 *   present.  A missing device (fd < 0) is treated as success so the
 *   HAL degrades gracefully off-target.
 *
 ****************************************************************************/

static int camera_hal_set_format(struct camera_context_s *ctx,
                                  const struct camera_pipe_config_s *cfg)
{
  struct v4l2_format fmt;

  if (ctx->fd < 0)
    {
      return 0;
    }

  memset(&fmt, 0, sizeof(fmt));
  fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width       = cfg->width;
  fmt.fmt.pix.height      = cfg->height;
  fmt.fmt.pix.pixelformat = camera_hal_v4l2_pixfmt(cfg->format);
  fmt.fmt.pix.field       = V4L2_FIELD_ANY;

  if (ioctl(ctx->fd, VIDIOC_S_FMT, (unsigned long)&fmt) < 0)
    {
      syslog(LOG_ERR, "camera: VIDIOC_S_FMT failed: %d\n", errno);
      return -errno;
    }

  return 0;
}

int camera_hal_config_display(struct camera_context_s *ctx,
                              const struct camera_pipe_config_s *cfg)
{
  if (!ctx->initialized)
    {
      return -1;
    }

  ctx->display_pipe = *cfg;

  syslog(LOG_INFO, "camera: display pipe %lux%lu fmt=%u\n",
         (unsigned long)cfg->width,
         (unsigned long)cfg->height, cfg->format);

  return camera_hal_set_format(ctx, cfg);
}

int camera_hal_config_nn(struct camera_context_s *ctx,
                         const struct camera_pipe_config_s *cfg)
{
  if (!ctx->initialized)
    {
      return -1;
    }

  ctx->nn_pipe = *cfg;

  syslog(LOG_INFO, "camera: NN pipe %lux%lu fmt=%u\n",
         (unsigned long)cfg->width,
         (unsigned long)cfg->height, cfg->format);

  return camera_hal_set_format(ctx, cfg);
}

int camera_hal_start(struct camera_context_s *ctx, int pipe,
                     void *buffer, int mode)
{
  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  if (!ctx->initialized)
    {
      return -1;
    }

  UNUSED(buffer);

  if (ctx->fd >= 0)
    {
      if (ioctl(ctx->fd, VIDIOC_STREAMON,
                (unsigned long)&type) < 0)
        {
          syslog(LOG_ERR, "camera: pipe %d STREAMON failed: %d\n",
                 pipe, errno);
          return -errno;
        }
    }

  syslog(LOG_INFO, "camera: pipe %d started (%s)\n",
         pipe,
         mode == CAM_MODE_CONTINUOUS ? "continuous" : "snapshot");
  return 0;
}

int camera_hal_stop(struct camera_context_s *ctx, int pipe)
{
  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  if (!ctx->initialized)
    {
      return -1;
    }

  if (ctx->fd >= 0)
    {
      if (ioctl(ctx->fd, VIDIOC_STREAMOFF,
                (unsigned long)&type) < 0)
        {
          syslog(LOG_ERR, "camera: pipe %d STREAMOFF failed: %d\n",
                 pipe, errno);
          return -errno;
        }
    }

  syslog(LOG_INFO, "camera: pipe %d stopped\n", pipe);
  return 0;
}

void camera_hal_isp_update(struct camera_context_s *ctx)
{
  if (!ctx->initialized)
    {
      return;
    }

  /* ISP auto-exposure / auto-white-balance runs inside the driver;
   * nothing to drive from user space through the V4L2 interface.
   */
}

int camera_hal_set_callback(struct camera_context_s *ctx, int pipe,
                            camera_frame_cb_t cb, void *arg)
{
  if (!ctx->initialized || pipe > 1)
    {
      return -1;
    }

  g_callbacks[pipe] = cb;
  g_cb_args[pipe] = arg;
  return 0;
}

void camera_hal_deinit(struct camera_context_s *ctx)
{
  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  if (!ctx->initialized)
    {
      return;
    }

  if (ctx->fd >= 0)
    {
      ioctl(ctx->fd, VIDIOC_STREAMOFF, (unsigned long)&type);
      close(ctx->fd);
    }

  memset(ctx, 0, sizeof(*ctx));
  ctx->fd = -1;
  syslog(LOG_INFO, "camera: deinitialized\n");
}
