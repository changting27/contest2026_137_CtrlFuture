/****************************************************************************
 * app/edgesight/config.h
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
 * EdgeSight - Runtime configuration management.
 * Default values can be overridden from SD card config file.
 *
 ****************************************************************************/

#ifndef __APP_EDGESIGHT_CONFIG_H
#define __APP_EDGESIGHT_CONFIG_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include <stdbool.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define CONFIG_FILE_PATH       "/mnt/sd/edgesight.cfg"
#define CONFIG_KEY_MAX         32
#define CONFIG_VALUE_MAX       64

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* Fall detection tuning parameters */

struct fall_config_s
{
  float    torso_angle_thresh;    /* degrees, default 45.0 */
  float    cog_ratio_thresh;      /* 0-1, default 0.5 */
  float    bbox_ratio_thresh;     /* w/h, default 1.2 */
  float    confidence_thresh;     /* 0-1, default 0.6 */
  uint8_t  consecutive_frames;    /* default 3 */
};

/* Recording parameters */

struct record_config_s
{
  uint32_t max_duration_s;        /* default 30 */
  uint32_t pre_event_s;           /* default 5 */
  uint32_t bitrate_kbps;          /* default 2000 */
  uint8_t  fps;                   /* default 15 */
  uint8_t  gop_size;              /* default 30 */
};

/* Network parameters */

struct network_settings_s
{
  char     mqtt_broker[48];       /* default "192.168.1.1" */
  uint16_t mqtt_port;             /* default 1883 */
  char     mqtt_topic[48];        /* default "edgesight/alert" */
  char     device_id[24];         /* default "edgesight-001" */
  uint8_t  mqtt_qos;              /* default 1 */
};

/* Display parameters */

struct display_settings_s
{
  bool     show_bbox;             /* default true */
  bool     show_skeleton;         /* default true */
  bool     show_stats;            /* default true */
  bool     show_alert;            /* default true */
  uint8_t  alert_duration_s;      /* default 5 */
};

/* Top-level configuration */

struct edgesight_config_s
{
  struct fall_config_s      fall;
  struct record_config_s    record;
  struct network_settings_s network;
  struct display_settings_s display;
  uint8_t  log_level;             /* EVENT_LEVEL_xxx */
  bool     auto_record;           /* default true */
  bool     auto_alert;            /* default true */
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/**
 * @brief Initialize configuration with defaults
 * @param cfg Configuration structure to fill
 */

void config_set_defaults(struct edgesight_config_s *cfg);

/**
 * @brief Load configuration from SD card file
 * @param cfg Configuration structure to update
 * @param path Config file path (NULL = default)
 * @return 0 on success, -1 file not found, -2 parse error
 */

int config_load_file(struct edgesight_config_s *cfg,
                     const char *path);

/**
 * @brief Save current configuration to SD card
 * @param cfg Configuration to save
 * @param path Config file path (NULL = default)
 * @return 0 on success, -1 on error
 */

int config_save_file(const struct edgesight_config_s *cfg,
                     const char *path);

/**
 * @brief Print current configuration to console
 * @param cfg Configuration to print
 */

void config_dump(const struct edgesight_config_s *cfg);

#endif /* __APP_EDGESIGHT_CONFIG_H */
