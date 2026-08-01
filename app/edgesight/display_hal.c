/****************************************************************************
 * app/edgesight/display_hal.c
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
 * EdgeSight - Display HAL implementation.
 *
 * Talks to the display through the standard NuttX framebuffer
 * interface (/dev/fb0) only.  Bounding boxes and overlays are drawn
 * with a portable software rectangle fill, so the HAL never depends
 * on chip private headers and stays usable off-target.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "display_hal.h"
#include "memory_map.h"
#include <string.h>
#include <stdio.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <nuttx/video/fb.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define DISPLAY_HAL_DEVPATH "/dev/fb0"

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: display_fill_rect
 *
 * Description:
 *   Software fill of an RGB565 rectangle into the mapped framebuffer.
 *   Clips to the framebuffer bounds.  No-op when no device is mapped.
 *
 ****************************************************************************/

static void display_fill_rect(struct display_context_s *ctx,
                              uint32_t x, uint32_t y,
                              uint32_t w, uint32_t h,
                              uint16_t color)
{
  uint32_t row;
  uint32_t col;
  uint32_t xmax;
  uint32_t ymax;

  if (ctx->fbmem == NULL || ctx->fbbpp != 16)
    {
      return;
    }

  xmax = x + w;
  ymax = y + h;

  if (xmax > ctx->config.screen_width)
    {
      xmax = ctx->config.screen_width;
    }

  if (ymax > ctx->config.screen_height)
    {
      ymax = ctx->config.screen_height;
    }

  for (row = y; row < ymax; row++)
    {
      FAR uint16_t *line = (FAR uint16_t *)
        ((FAR uint8_t *)ctx->fbmem + row * ctx->fbstride);

      for (col = x; col < xmax; col++)
        {
          line[col] = color;
        }
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int display_hal_init(struct display_context_s *ctx,
                     const struct display_config_s *cfg)
{
  struct fb_videoinfo_s vinfo;
  struct fb_planeinfo_s pinfo;

  memset(ctx, 0, sizeof(*ctx));
  ctx->config = *cfg;
  ctx->fd = -1;

  /* Background buffer is provided later via display_hal_set_bg_buffer()
   * or taken from the framebuffer plane below; no board-private address
   * is referenced here.
   */

  /* Open the framebuffer.  A missing node is non-fatal so the HAL
   * degrades gracefully on hosts and boards without a panel.
   */

  ctx->fd = open(DISPLAY_HAL_DEVPATH, O_RDWR);
  if (ctx->fd < 0)
    {
      syslog(LOG_WARNING, "display: %s unavailable (%d), "
             "running headless\n", DISPLAY_HAL_DEVPATH, errno);
    }
  else
    {
      memset(&vinfo, 0, sizeof(vinfo));
      memset(&pinfo, 0, sizeof(pinfo));

      if (ioctl(ctx->fd, FBIOGET_VIDEOINFO,
                (unsigned long)&vinfo) == 0 &&
          ioctl(ctx->fd, FBIOGET_PLANEINFO,
                (unsigned long)&pinfo) == 0)
        {
          ctx->fbmem    = pinfo.fbmem;
          ctx->fbstride = pinfo.stride;
          ctx->fbbpp    = pinfo.bpp;
        }
      else
        {
          syslog(LOG_ERR, "display: framebuffer query failed: %d\n",
                 errno);
        }
    }

  ctx->fg_write_idx = 0;
  ctx->initialized = true;

  syslog(LOG_INFO, "display: initialized %lux%lu\n",
         (unsigned long)cfg->screen_width,
         (unsigned long)cfg->screen_height);
  return 0;
}

void display_hal_set_bg_buffer(struct display_context_s *ctx,
                               void *buffer)
{
  if (!ctx->initialized)
    {
      return;
    }

  ctx->bg_buffer = buffer;

  /* The camera feed is delivered straight into the framebuffer by
   * the capture pipeline; nothing to program from user space here.
   */
}

void display_hal_clear_fg(struct display_context_s *ctx)
{
  if (!ctx->initialized)
    {
      return;
    }

  /* Redraw the background feed to erase the previous overlay.  When
   * a background buffer is present, copy it back into the visible
   * framebuffer; otherwise leave the camera plane untouched.
   */

  if (ctx->fbmem != NULL && ctx->bg_buffer != NULL &&
      ctx->bg_buffer != ctx->fbmem)
    {
      size_t span = (size_t)ctx->fbstride * ctx->config.screen_height;
      memcpy(ctx->fbmem, ctx->bg_buffer, span);
    }
}

void display_hal_draw_bbox(struct display_context_s *ctx,
                           const struct display_bbox_s *bbox)
{
  if (!ctx->initialized)
    {
      return;
    }

  /* Four edges of the rectangle (2px thick) in the bbox color. */

  display_fill_rect(ctx, bbox->x, bbox->y, bbox->w, 2,
                    (uint16_t)bbox->color);
  display_fill_rect(ctx, bbox->x, bbox->y + bbox->h - 2, bbox->w, 2,
                    (uint16_t)bbox->color);
  display_fill_rect(ctx, bbox->x, bbox->y, 2, bbox->h,
                    (uint16_t)bbox->color);
  display_fill_rect(ctx, bbox->x + bbox->w - 2, bbox->y, 2, bbox->h,
                    (uint16_t)bbox->color);
}

void display_hal_draw_stats(struct display_context_s *ctx,
                            const struct display_stats_s *stats)
{
  if (!ctx->initialized)
    {
      return;
    }

  /* Stats panel background (dark bar along the top). */

  display_fill_rect(ctx, 0, 0, ctx->config.screen_width, 24, 0x2104);

  /* TODO: Draw text using font rendering.  For now the bar marks the
   * stats area; full text needs a font bitmap or GPU2D text.
   */

  (void)stats;
}

void display_hal_show_alert(struct display_context_s *ctx,
                            const char *msg)
{
  uint32_t y;

  if (!ctx->initialized)
    {
      return;
    }

  /* Red alert bar across the bottom of the screen. */

  y = ctx->config.screen_height - 40;
  display_fill_rect(ctx, 0, y, ctx->config.screen_width, 40, 0xf800);

  /* TODO: Draw alert text on top of the red bar. */

  syslog(LOG_WARNING, "display: ALERT: %s\n", msg);
}

void display_hal_swap(struct display_context_s *ctx)
{
  struct fb_planeinfo_s pinfo;

  if (!ctx->initialized)
    {
      return;
    }

  /* Commit the current frame.  FBIOPAN_DISPLAY is best-effort: not
   * every driver supports panning, and a failure is not fatal.
   */

  if (ctx->fd >= 0)
    {
      memset(&pinfo, 0, sizeof(pinfo));
      pinfo.yoffset = 0;
      ioctl(ctx->fd, FBIOPAN_DISPLAY, (unsigned long)&pinfo);
    }

  ctx->fg_write_idx = 1 - ctx->fg_write_idx;
}

void display_hal_deinit(struct display_context_s *ctx)
{
  if (!ctx->initialized)
    {
      return;
    }

  if (ctx->fd >= 0)
    {
      close(ctx->fd);
    }

  memset(ctx, 0, sizeof(*ctx));
  ctx->fd = -1;
  syslog(LOG_INFO, "display: deinitialized\n");
}
