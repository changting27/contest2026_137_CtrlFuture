/****************************************************************************
 * app/edgesight/camera_hal.h
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
 * EdgeSight - Camera hardware abstraction layer.
 * Wraps DCMIPP dual-pipeline architecture for NuttX integration.
 *
 ****************************************************************************/

#ifndef __APP_EDGESIGHT_CAMERA_HAL_H
#define __APP_EDGESIGHT_CAMERA_HAL_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include <stdbool.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Pixel formats */

#define CAM_FMT_RGB565      0
#define CAM_FMT_RGB888      1
#define CAM_FMT_YUV422      2
#define CAM_FMT_YUV420      3

/* Capture modes */

#define CAM_MODE_CONTINUOUS 0  /* Frames delivered continuously */
#define CAM_MODE_SNAPSHOT   1  /* Single frame capture */

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* Camera pipeline configuration */

struct camera_pipe_config_s
{
  uint32_t width;       /* Output width (0 = sensor default) */
  uint32_t height;      /* Output height (0 = sensor default) */
  uint8_t  format;      /* Output pixel format (CAM_FMT_xxx) */
  uint8_t  bpp;         /* Bytes per pixel */
  bool     enable_isp;  /* Enable ISP processing */
};

/* Camera context */

struct camera_context_s
{
  bool initialized;
  int  fd;                 /* /dev/video0 handle, -1 if unavailable */
  uint32_t sensor_width;   /* Native sensor width */
  uint32_t sensor_height;  /* Native sensor height */
  uint32_t fps;
  struct camera_pipe_config_s display_pipe; /* PIPE1: display */
  struct camera_pipe_config_s nn_pipe;      /* PIPE2: NN input */
};

/* Frame callback type */

typedef void (*camera_frame_cb_t)(int pipe, void *buffer,
                                  uint32_t size, void *arg);

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/**
 * @brief Initialize camera + DCMIPP + ISP
 * @param ctx    Camera context
 * @param fps    Target frame rate
 * @return 0 on success, negative errno on failure
 */

int camera_hal_init(struct camera_context_s *ctx, uint32_t fps);

/**
 * @brief Configure display pipeline (PIPE1)
 * @param ctx    Camera context
 * @param cfg    Pipeline configuration
 * @return 0 on success
 */

int camera_hal_config_display(struct camera_context_s *ctx,
                              const struct camera_pipe_config_s *cfg);

/**
 * @brief Configure NN inference pipeline (PIPE2)
 * @param ctx    Camera context
 * @param cfg    Pipeline configuration
 * @return 0 on success
 */

int camera_hal_config_nn(struct camera_context_s *ctx,
                         const struct camera_pipe_config_s *cfg);

/**
 * @brief Start capture on a pipeline
 * @param ctx    Camera context
 * @param pipe   0=display, 1=NN
 * @param buffer Destination buffer
 * @param mode   CAM_MODE_CONTINUOUS or CAM_MODE_SNAPSHOT
 * @return 0 on success
 */

int camera_hal_start(struct camera_context_s *ctx, int pipe,
                     void *buffer, int mode);

/**
 * @brief Stop capture on a pipeline
 * @param ctx    Camera context
 * @param pipe   0=display, 1=NN
 * @return 0 on success
 */

int camera_hal_stop(struct camera_context_s *ctx, int pipe);

/**
 * @brief Run ISP update (call periodically for AE/AWB)
 * @param ctx    Camera context
 */

void camera_hal_isp_update(struct camera_context_s *ctx);

/**
 * @brief Register frame-received callback
 * @param ctx    Camera context
 * @param pipe   0=display, 1=NN
 * @param cb     Callback function
 * @param arg    User argument passed to callback
 */

int camera_hal_set_callback(struct camera_context_s *ctx, int pipe,
                            camera_frame_cb_t cb, void *arg);

/**
 * @brief Deinitialize camera
 */

void camera_hal_deinit(struct camera_context_s *ctx);

#endif /* __APP_EDGESIGHT_CAMERA_HAL_H */
