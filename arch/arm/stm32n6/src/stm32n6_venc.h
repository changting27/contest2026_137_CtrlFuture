/****************************************************************************
 * arch/arm/src/stm32n6/stm32n6_venc.h
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
 ****************************************************************************/

#ifndef __ARCH_ARM_SRC_STM32N6_STM32N6_VENC_H
#define __ARCH_ARM_SRC_STM32N6_STM32N6_VENC_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Name: stm32n6_venc_register
 *
 * Description:
 *   Register the STM32N6 VENC (H.264/JPEG hardware encoder) as a V4L2
 *   memory-to-memory codec device.  The OUTPUT queue accepts raw YUV
 *   frames; the CAPTURE queue produces the encoded H.264 bitstream.
 *
 *   The video framework (drivers/video/v4l2_m2m.c) owns /dev/videoN and
 *   every VIDIOC_* ioctl; this backend supplies the codec_ops_s vtable
 *   the framework binds against.
 *
 * Input Parameters:
 *   devpath - Device node path (e.g. "/dev/video1").
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno value on failure.
 *
 ****************************************************************************/

int stm32n6_venc_register(const char *devpath);

#endif /* __ARCH_ARM_SRC_STM32N6_STM32N6_VENC_H */
