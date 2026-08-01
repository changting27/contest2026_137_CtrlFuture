/****************************************************************************
 * arch/arm/src/stm32n6/stm32n6_ltdc.h
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
 * STM32N6 LTDC display controller driver header.
 *
 ****************************************************************************/

#ifndef __ARCH_ARM_SRC_STM32N6_STM32N6_LTDC_H
#define __ARCH_ARM_SRC_STM32N6_STM32N6_LTDC_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdint.h>

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/* The LTDC controller is exposed through the standard NuttX framebuffer
 * framework.  It implements the framework entry points up_fbinitialize(),
 * up_fbgetvplane() and up_fbuninitialize() (declared in
 * <nuttx/video/fb.h>); board bringup creates /dev/fb0 by calling
 * fb_register(0, 0).  No chip-specific API is published here.
 */

#endif /* __ARCH_ARM_SRC_STM32N6_STM32N6_LTDC_H */
