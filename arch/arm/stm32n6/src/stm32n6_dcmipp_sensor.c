/****************************************************************************
 * arch/arm/src/stm32n6/stm32n6_dcmipp_sensor.c
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
 * STM32N6 camera image sensor (imgsensor) backend.
 *
 * The NuttX video framework (drivers/video/v4l2_cap.c) binds an imgdata
 * backend (stm32n6_dcmipp.c) with an imgsensor backend to create a
 * /dev/videoN capture device.  This file provides the sensor half: it
 * advertises the frame formats/sizes the board camera can deliver and
 * accepts the framework's stream control calls.
 *
 * The sensor I2C bring-up (identify chip, load the register profile,
 * program exposure/gain) is marked TODO(sensor): it needs the concrete
 * module part (e.g. IMX335/OV5640) and its register map, which are not
 * yet available in-tree.  The framework contract is complete so the
 * pipeline binds and streams once the I2C profile lands.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/video/imgsensor.h>
#include <sys/videoio.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>

#include "stm32n6_dcmipp.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define SENSOR_DRIVER_NAME     "stm32n6-camera"

/* Default geometry advertised to the framework.  800x480 matches the
 * EdgeSight display feed; the NN pipe crops from the same stream.
 */

#define SENSOR_DEF_WIDTH       800
#define SENSOR_DEF_HEIGHT      480
#define SENSOR_DEF_FPS_NUM     1
#define SENSOR_DEF_FPS_DEN     30

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static bool stm32n6_sensor_is_available(FAR struct imgsensor_s *sensor);
static int  stm32n6_sensor_init(FAR struct imgsensor_s *sensor);
static int  stm32n6_sensor_uninit(FAR struct imgsensor_s *sensor);
static FAR const char *
stm32n6_sensor_get_driver_name(FAR struct imgsensor_s *sensor);
static int  stm32n6_sensor_validate_frame_setting(
                                FAR struct imgsensor_s *sensor,
                                imgsensor_stream_type_t type,
                                uint8_t nr_datafmts,
                                FAR imgsensor_format_t *datafmts,
                                FAR imgsensor_interval_t *interval);
static int  stm32n6_sensor_start_capture(FAR struct imgsensor_s *sensor,
                                imgsensor_stream_type_t type,
                                uint8_t nr_datafmts,
                                FAR imgsensor_format_t *datafmts,
                                FAR imgsensor_interval_t *interval);
static int  stm32n6_sensor_stop_capture(FAR struct imgsensor_s *sensor,
                                imgsensor_stream_type_t type);
static int  stm32n6_sensor_get_supported_value(
                                FAR struct imgsensor_s *sensor,
                                uint32_t id,
                                FAR imgsensor_supported_value_t *value);

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Pixel formats the pipeline can deliver. */

static const struct v4l2_fmtdesc g_stm32n6_sensor_fmtdescs[] =
{
  {
    .index       = 0,
    .type        = V4L2_BUF_TYPE_VIDEO_CAPTURE,
    .pixelformat = V4L2_PIX_FMT_RGB565,
    .description = "RGB565",
  },
  {
    .index       = 1,
    .type        = V4L2_BUF_TYPE_VIDEO_CAPTURE,
    .pixelformat = V4L2_PIX_FMT_UYVY,
    .description = "UYVY",
  },
};

/* Discrete frame sizes advertised for each format. */

static const struct v4l2_frmsizeenum g_stm32n6_sensor_frmsizes[] =
{
  {
    .index             = 0,
    .buf_type          = V4L2_BUF_TYPE_VIDEO_CAPTURE,
    .pixel_format      = V4L2_PIX_FMT_RGB565,
    .type              = V4L2_FRMSIZE_TYPE_DISCRETE,
    .discrete          =
      {
        .width  = SENSOR_DEF_WIDTH,
        .height = SENSOR_DEF_HEIGHT,
      },
  },
};

static const struct imgsensor_ops_s g_stm32n6_sensor_ops =
{
  .is_available           = stm32n6_sensor_is_available,
  .init                   = stm32n6_sensor_init,
  .uninit                 = stm32n6_sensor_uninit,
  .get_driver_name        = stm32n6_sensor_get_driver_name,
  .validate_frame_setting = stm32n6_sensor_validate_frame_setting,
  .start_capture          = stm32n6_sensor_start_capture,
  .stop_capture           = stm32n6_sensor_stop_capture,
  .get_supported_value    = stm32n6_sensor_get_supported_value,
};

static struct imgsensor_s g_stm32n6_sensor =
{
  .ops              = &g_stm32n6_sensor_ops,
  .fmtdescs_num     = sizeof(g_stm32n6_sensor_fmtdescs) /
                      sizeof(g_stm32n6_sensor_fmtdescs[0]),
  .fmtdescs         = g_stm32n6_sensor_fmtdescs,
  .frmsizes_num     = sizeof(g_stm32n6_sensor_frmsizes) /
                      sizeof(g_stm32n6_sensor_frmsizes[0]),
  .frmsizes         = g_stm32n6_sensor_frmsizes,
  .frmintervals_num = 0,
  .frmintervals     = NULL,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: stm32n6_sensor_is_available
 *
 * Description:
 *   Report the sensor as present so the framework binds it.  When the
 *   real module is known this should probe the sensor over I2C.
 *
 ****************************************************************************/

static bool stm32n6_sensor_is_available(FAR struct imgsensor_s *sensor)
{
  UNUSED(sensor);

  /* TODO(sensor): probe the module chip-id over I2C and return the
   * result instead of an unconditional true.
   */

  return true;
}

/****************************************************************************
 * Name: stm32n6_sensor_init / uninit
 ****************************************************************************/

static int stm32n6_sensor_init(FAR struct imgsensor_s *sensor)
{
  UNUSED(sensor);

  /* TODO(sensor): reset the module, load its base register profile and
   * program the clock lanes over I2C.
   */

  syslog(LOG_INFO, "dcmipp-sensor: initialized\n");
  return OK;
}

static int stm32n6_sensor_uninit(FAR struct imgsensor_s *sensor)
{
  UNUSED(sensor);
  return OK;
}

/****************************************************************************
 * Name: stm32n6_sensor_get_driver_name
 ****************************************************************************/

static FAR const char *
stm32n6_sensor_get_driver_name(FAR struct imgsensor_s *sensor)
{
  UNUSED(sensor);
  return SENSOR_DRIVER_NAME;
}

/****************************************************************************
 * Name: stm32n6_sensor_validate_frame_setting
 *
 * Description:
 *   Accept the requested geometry when it matches an advertised format.
 *
 ****************************************************************************/

static int stm32n6_sensor_validate_frame_setting(
                                FAR struct imgsensor_s *sensor,
                                imgsensor_stream_type_t type,
                                uint8_t nr_datafmts,
                                FAR imgsensor_format_t *datafmts,
                                FAR imgsensor_interval_t *interval)
{
  UNUSED(sensor);
  UNUSED(type);
  UNUSED(interval);

  if (nr_datafmts < 1 || datafmts == NULL)
    {
      return -EINVAL;
    }

  if (datafmts[IMGSENSOR_FMT_MAIN].width == 0 ||
      datafmts[IMGSENSOR_FMT_MAIN].height == 0)
    {
      return -EINVAL;
    }

  return OK;
}

/****************************************************************************
 * Name: stm32n6_sensor_start_capture / stop_capture
 ****************************************************************************/

static int stm32n6_sensor_start_capture(FAR struct imgsensor_s *sensor,
                                imgsensor_stream_type_t type,
                                uint8_t nr_datafmts,
                                FAR imgsensor_format_t *datafmts,
                                FAR imgsensor_interval_t *interval)
{
  UNUSED(sensor);
  UNUSED(type);
  UNUSED(nr_datafmts);
  UNUSED(datafmts);
  UNUSED(interval);

  /* TODO(sensor): program the module output window / format and start
   * streaming over I2C.
   */

  return OK;
}

static int stm32n6_sensor_stop_capture(FAR struct imgsensor_s *sensor,
                                imgsensor_stream_type_t type)
{
  UNUSED(sensor);
  UNUSED(type);

  /* TODO(sensor): stop the module output stream over I2C. */

  return OK;
}

/****************************************************************************
 * Name: stm32n6_sensor_get_supported_value
 *
 * Description:
 *   No adjustable controls are exposed yet; report none so the framework
 *   does not attempt to program them.
 *
 ****************************************************************************/

static int stm32n6_sensor_get_supported_value(
                                FAR struct imgsensor_s *sensor,
                                uint32_t id,
                                FAR imgsensor_supported_value_t *value)
{
  UNUSED(sensor);
  UNUSED(id);
  UNUSED(value);

  /* TODO(sensor): advertise brightness/exposure/gain ranges once the
   * module control set is known.
   */

  return -EINVAL;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: stm32n6_dcmipp_sensor_register
 ****************************************************************************/

int stm32n6_dcmipp_sensor_register(void)
{
  imgsensor_register(&g_stm32n6_sensor);
  syslog(LOG_INFO, "dcmipp-sensor: registered imgsensor backend\n");
  return OK;
}
