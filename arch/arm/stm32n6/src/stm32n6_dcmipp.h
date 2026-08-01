/****************************************************************************
 * arch/arm/src/stm32n6/stm32n6_dcmipp.h
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
 * STM32N6 DCMIPP camera driver header.
 *
 ****************************************************************************/

#ifndef __ARCH_ARM_SRC_STM32N6_STM32N6_DCMIPP_H
#define __ARCH_ARM_SRC_STM32N6_STM32N6_DCMIPP_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define DCMIPP_MODE_CONTINUOUS   0
#define DCMIPP_MODE_SNAPSHOT     1

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Name: stm32n6_dcmipp_register
 *
 * Description:
 *   Register the DCMIPP capture engine as the V4L2 image data (imgdata)
 *   backend.  The video framework binds this backend together with a
 *   registered image sensor when capture_register() creates /dev/videoN.
 *
 * Returned Value:
 *   OK on success; a negated errno value on failure.
 *
 ****************************************************************************/

int stm32n6_dcmipp_register(void);

/****************************************************************************
 * Name: stm32n6_dcmipp_sensor_register
 *
 * Description:
 *   Register the board camera as the V4L2 image sensor (imgsensor)
 *   backend.  Must be called before capture_register() so the video
 *   framework can bind a sensor to the DCMIPP imgdata backend.
 *
 * Returned Value:
 *   OK on success; a negated errno value on failure.
 *
 ****************************************************************************/

int stm32n6_dcmipp_sensor_register(void);

/****************************************************************************
 * Name: stm32n6_dcmipp_frame_event
 *
 * Description:
 *   Notify the imgdata backend that a capture DMA transfer has completed.
 *   Intended to be called from the DCMIPP frame interrupt handler.  The
 *   stored video-framework capture callback is invoked so the frame is
 *   handed back to the V4L2 core.
 *
 * Input Parameters:
 *   pipe - Hardware pipe index that produced the frame.
 *
 ****************************************************************************/

void stm32n6_dcmipp_frame_event(uint32_t pipe);

#endif /* __ARCH_ARM_SRC_STM32N6_STM32N6_DCMIPP_H */
