/****************************************************************************
 * app/edgesight/display_hal.h
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
 * EdgeSight - Display hardware abstraction layer.
 * Wraps LTDC dual-layer + GPU2D rendering for NuttX integration.
 *
 ****************************************************************************/

#ifndef __APP_EDGESIGHT_DISPLAY_HAL_H
#define __APP_EDGESIGHT_DISPLAY_HAL_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include <stdbool.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Layer indices */

#define DISPLAY_LAYER_BG    0  /* Background: camera live feed */
#define DISPLAY_LAYER_FG    1  /* Foreground: detection overlay + UI */

/* Colors (ARGB8888) */

#define DISPLAY_COLOR_TRANSPARENT  0x00000000
#define DISPLAY_COLOR_RED          0xFFFF0000
#define DISPLAY_COLOR_GREEN        0xFF00FF00
#define DISPLAY_COLOR_BLUE         0xFF0000FF
#define DISPLAY_COLOR_WHITE        0xFFFFFFFF
#define DISPLAY_COLOR_BLACK        0xFF000000
#define DISPLAY_COLOR_YELLOW       0xFFFFFF00
#define DISPLAY_COLOR_ALERT_BG     0x80FF0000  /* semi-transparent red */

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* Display configuration */

struct display_config_s
{
  uint32_t screen_width;
  uint32_t screen_height;
  uint32_t bg_format;    /* RGB565 for camera feed */
  uint32_t fg_format;    /* ARGB4444 for overlay */
};

/* Bounding box for drawing */

struct display_bbox_s
{
  uint32_t x;
  uint32_t y;
  uint32_t w;
  uint32_t h;
  uint32_t color;
  const char *label;
  float confidence;
};

/* Display statistics overlay */

struct display_stats_s
{
  uint32_t fps;
  uint32_t inference_ms;
  uint32_t persons_detected;
  uint32_t fall_count;
  bool alert_active;
  float temperature;
  float humidity;
};

/* Display context */

struct display_context_s
{
  bool initialized;
  struct display_config_s config;
  void *bg_buffer;       /* Background framebuffer */
  void *fg_buffer[2];    /* Double-buffered foreground */
  int fg_write_idx;      /* Current write buffer index */
  int fd;                /* /dev/fb0 handle, -1 if unavailable */
  void *fbmem;           /* Mapped framebuffer memory, NULL if none */
  uint32_t fbstride;     /* Framebuffer row stride in bytes */
  uint8_t fbbpp;         /* Framebuffer bits per pixel */
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/**
 * @brief Initialize LTDC + GPU2D display system
 * @param ctx    Display context
 * @param cfg    Display configuration
 * @return 0 on success, negative errno on failure
 */

int display_hal_init(struct display_context_s *ctx,
                     const struct display_config_s *cfg);

/**
 * @brief Set the background layer buffer (camera feed)
 * @param ctx    Display context
 * @param buffer Pointer to camera framebuffer (RGB565)
 */

void display_hal_set_bg_buffer(struct display_context_s *ctx,
                               void *buffer);

/**
 * @brief Clear the foreground overlay
 * @param ctx    Display context
 */

void display_hal_clear_fg(struct display_context_s *ctx);

/**
 * @brief Draw a bounding box on foreground overlay
 * @param ctx    Display context
 * @param bbox   Bounding box parameters
 */

void display_hal_draw_bbox(struct display_context_s *ctx,
                           const struct display_bbox_s *bbox);

/**
 * @brief Draw statistics overlay panel
 * @param ctx    Display context
 * @param stats  Statistics to display
 */

void display_hal_draw_stats(struct display_context_s *ctx,
                            const struct display_stats_s *stats);

/**
 * @brief Show full-screen alert (fall detected)
 * @param ctx    Display context
 * @param msg    Alert message string
 */

void display_hal_show_alert(struct display_context_s *ctx,
                            const char *msg);

/**
 * @brief Swap foreground buffers (commit current frame to display)
 * @param ctx    Display context
 */

void display_hal_swap(struct display_context_s *ctx);

/**
 * @brief Deinitialize display
 */

void display_hal_deinit(struct display_context_s *ctx);

#endif /* __APP_EDGESIGHT_DISPLAY_HAL_H */
