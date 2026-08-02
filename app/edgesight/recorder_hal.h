/****************************************************************************
 * app/edgesight/recorder_hal.h
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
 * EdgeSight - Event recorder hardware abstraction layer.
 * Wraps H.264 VENC hardware encoder + SD card storage.
 *
 ****************************************************************************/

#ifndef __APP_EDGESIGHT_RECORDER_HAL_H
#define __APP_EDGESIGHT_RECORDER_HAL_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include <stdbool.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Recording resolution presets */

#define REC_RES_480P     0  /* 640x480 */
#define REC_RES_720P     1  /* 1280x720 */
#define REC_RES_1080P    2  /* 1920x1080 */

/* Recording state */

#define REC_STATE_IDLE      0
#define REC_STATE_RECORDING 1
#define REC_STATE_STOPPING  2
#define REC_STATE_ERROR     3

/* Default recording parameters */

#define REC_DEFAULT_DURATION_S   30   /* seconds per event clip */
#define REC_DEFAULT_BITRATE      2000 /* kbps */
#define REC_DEFAULT_FPS          15
#define REC_DEFAULT_GOP          30   /* I-frame interval */

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* Recorder configuration */

struct recorder_config_s
{
  uint8_t  resolution;       /* REC_RES_xxx */
  uint32_t bitrate_kbps;     /* Target bitrate */
  uint32_t fps;              /* Target frame rate */
  uint32_t gop_size;         /* Group-of-pictures (I-frame interval) */
  uint32_t max_duration_s;   /* Max clip duration in seconds */
  const char *output_dir;    /* SD card directory for clips */
};

/* Recorder statistics */

struct recorder_stats_s
{
  uint32_t state;
  uint32_t frames_encoded;
  uint32_t bytes_written;
  uint32_t elapsed_ms;
  uint32_t avg_bitrate_kbps;
  uint32_t dropped_frames;
};

/* Recorder context */

struct recorder_context_s
{
  bool initialized;
  struct recorder_config_s config;
  uint32_t state;
  uint32_t frame_count;
  uint32_t start_tick;
  int output_fd;            /* Current .h264 output file descriptor */
  int codec_fd;             /* V4L2 M2M encoder node (/dev/video1), or -1 */
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/**
 * @brief Initialize H.264 encoder and SD card storage
 * @param ctx    Recorder context
 * @param cfg    Recorder configuration
 * @return 0 on success, negative errno on failure
 */

int recorder_hal_init(struct recorder_context_s *ctx,
                      const struct recorder_config_s *cfg);

/**
 * @brief Start recording an event clip
 * @param ctx       Recorder context
 * @param event_id  Event identifier (used in filename)
 * @return 0 on success, negative errno on failure
 *
 * Creates file: <output_dir>/event_<id>_<timestamp>.h264
 */

int recorder_hal_start(struct recorder_context_s *ctx,
                       uint32_t event_id);

/**
 * @brief Feed a frame to the encoder
 * @param ctx    Recorder context
 * @param frame  YUV frame data from camera
 * @param size   Frame data size in bytes
 * @return 0 on success, negative errno on failure
 *
 * The encoder will produce H.264 NAL units written to the output file.
 */

int recorder_hal_feed_frame(struct recorder_context_s *ctx,
                            const void *frame, uint32_t size);

/**
 * @brief Stop recording and close the clip file
 * @param ctx    Recorder context
 * @return 0 on success
 */

int recorder_hal_stop(struct recorder_context_s *ctx);

/**
 * @brief Get current recording statistics
 * @param ctx    Recorder context
 * @param stats  Output statistics
 */

void recorder_hal_get_stats(const struct recorder_context_s *ctx,
                            struct recorder_stats_s *stats);

/**
 * @brief Deinitialize recorder and release resources
 */

void recorder_hal_deinit(struct recorder_context_s *ctx);

#endif /* __APP_EDGESIGHT_RECORDER_HAL_H */
