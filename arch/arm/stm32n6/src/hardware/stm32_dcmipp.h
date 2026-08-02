/****************************************************************************
 * arch/arm/src/stm32n6/hardware/stm32_dcmipp.h
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

#ifndef __ARCH_ARM_SRC_STM32N6_HARDWARE_STM32_DCMIPP_H
#define __ARCH_ARM_SRC_STM32N6_HARDWARE_STM32_DCMIPP_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include "hardware/stm32_memorymap.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Register offsets *********************************************************
 *
 * The DCMIPP has no in-tree reference driver; all offsets and bit values
 * below are taken from RM0486 section 39 (DCMIPP registers, 39.14.x) and
 * its register map (39.15).  Only the registers used by the minimal
 * parallel-input -> Pipe0 -> memory capture path are defined here; the
 * ISP stages (demosaic, colour conversion, statistics) and Pipe1/Pipe2
 * are intentionally left out until that functionality is implemented.
 */

/* IP-Plug (AXI master) global config */

#define STM32_DCMIPP_IPGR1_OFFSET    0x0000 /* IP-Plug global register 1 */

/* Parallel interface */

#define STM32_DCMIPP_PRCR_OFFSET     0x0104 /* Parallel interface control */
#define STM32_DCMIPP_PRESCR_OFFSET   0x0108 /* Parallel embedded sync code */
#define STM32_DCMIPP_PRESUR_OFFSET   0x010c /* Parallel emb sync unmask */

/* Common / capture-manager */

#define STM32_DCMIPP_CMCR_OFFSET     0x0204 /* Common configuration */
#define STM32_DCMIPP_CMIER_OFFSET    0x03f0 /* Common interrupt enable */
#define STM32_DCMIPP_CMSR1_OFFSET    0x03f4 /* Common status 1 */
#define STM32_DCMIPP_CMSR2_OFFSET    0x03f8 /* Common status 2 */
#define STM32_DCMIPP_CMFCR_OFFSET    0x03fc /* Common flag clear */

/* Pipe 0 (dump pipe: input bytes written straight to memory) */

#define STM32_DCMIPP_P0FSCR_OFFSET    0x0404 /* Pipe0 flow selection */
#define STM32_DCMIPP_P0FCTCR_OFFSET   0x0500 /* Pipe0 flow control */
#define STM32_DCMIPP_P0PPCR_OFFSET    0x05c0 /* Pipe0 pixel packer config */
#define STM32_DCMIPP_P0PPM0AR1_OFFSET 0x05c4 /* Pipe0 packer memory0 addr 1 */
#define STM32_DCMIPP_P0PPM0AR2_OFFSET 0x05c8 /* Pipe0 packer memory0 addr 2 */
#define STM32_DCMIPP_P0IER_OFFSET     0x05f4 /* Pipe0 interrupt enable */
#define STM32_DCMIPP_P0SR_OFFSET      0x05f8 /* Pipe0 status */
#define STM32_DCMIPP_P0FCR_OFFSET     0x05fc /* Pipe0 flag clear */

/* Register addresses *******************************************************/

#define STM32_DCMIPP_IPGR1     (STM32_DCMIPP_BASE + STM32_DCMIPP_IPGR1_OFFSET)
#define STM32_DCMIPP_PRCR      (STM32_DCMIPP_BASE + STM32_DCMIPP_PRCR_OFFSET)
#define STM32_DCMIPP_CMCR      (STM32_DCMIPP_BASE + STM32_DCMIPP_CMCR_OFFSET)
#define STM32_DCMIPP_CMIER     (STM32_DCMIPP_BASE + STM32_DCMIPP_CMIER_OFFSET)
#define STM32_DCMIPP_CMSR2     (STM32_DCMIPP_BASE + STM32_DCMIPP_CMSR2_OFFSET)
#define STM32_DCMIPP_CMFCR     (STM32_DCMIPP_BASE + STM32_DCMIPP_CMFCR_OFFSET)
#define STM32_DCMIPP_P0FSCR    (STM32_DCMIPP_BASE + \
                                STM32_DCMIPP_P0FSCR_OFFSET)
#define STM32_DCMIPP_P0FCTCR   (STM32_DCMIPP_BASE + \
                                STM32_DCMIPP_P0FCTCR_OFFSET)
#define STM32_DCMIPP_P0PPCR    (STM32_DCMIPP_BASE + \
                                STM32_DCMIPP_P0PPCR_OFFSET)
#define STM32_DCMIPP_P0PPM0AR1 (STM32_DCMIPP_BASE + \
                                STM32_DCMIPP_P0PPM0AR1_OFFSET)
#define STM32_DCMIPP_P0IER     (STM32_DCMIPP_BASE + STM32_DCMIPP_P0IER_OFFSET)
#define STM32_DCMIPP_P0SR      (STM32_DCMIPP_BASE + STM32_DCMIPP_P0SR_OFFSET)
#define STM32_DCMIPP_P0FCR     (STM32_DCMIPP_BASE + STM32_DCMIPP_P0FCR_OFFSET)

/* Register bit definitions *************************************************/

/* Parallel interface control register (DCMIPP_PRCR) */

#define DCMIPP_PRCR_ENABLE        (1 << 14) /* Parallel interface enable */
#define DCMIPP_PRCR_PCKPOL        (1 << 5)  /* Pixel clock polarity */
#define DCMIPP_PRCR_HSPOL         (1 << 6)  /* Horizontal sync polarity */
#define DCMIPP_PRCR_VSPOL         (1 << 7)  /* Vertical sync polarity */
#define DCMIPP_PRCR_EDM_SHIFT     (10)      /* Bits 10-12: ext data mode */
#define DCMIPP_PRCR_EDM_MASK      (0x7 << DCMIPP_PRCR_EDM_SHIFT)
#  define DCMIPP_PRCR_EDM_8BIT    (0x0 << DCMIPP_PRCR_EDM_SHIFT)
#define DCMIPP_PRCR_FORMAT_SHIFT  (16)      /* Bits 16-23: input format */
#define DCMIPP_PRCR_FORMAT_MASK   (0xff << DCMIPP_PRCR_FORMAT_SHIFT)
#  define DCMIPP_PRCR_FORMAT_RGB565 (0x22 << DCMIPP_PRCR_FORMAT_SHIFT)

/* Common configuration register (DCMIPP_CMCR) */

#define DCMIPP_CMCR_INSEL         (1 << 0)  /* Input selection (0=parallel) */

/* Pipe0 flow selection register (DCMIPP_P0FSCR) */

#define DCMIPP_P0FSCR_PIPEN       (1u << 31) /* Pipe0 enable */

/* Pipe0 flow control register (DCMIPP_P0FCTCR) */

#define DCMIPP_P0FCTCR_FRATE_SHIFT (0)       /* Bits 0-1: frame rate */
#define DCMIPP_P0FCTCR_FRATE_MASK  (0x3 << DCMIPP_P0FCTCR_FRATE_SHIFT)
#  define DCMIPP_P0FCTCR_FRATE_ALL (0x0 << DCMIPP_P0FCTCR_FRATE_SHIFT)
#define DCMIPP_P0FCTCR_CPTMODE     (1 << 2) /* 0=continuous, 1=snapshot */
#define DCMIPP_P0FCTCR_CPTREQ      (1 << 3) /* Capture request */

/* Pipe0 pixel packer config register (DCMIPP_P0PPCR).  Pipe0 is a raw
 * dump path: it writes the received bytes to memory with no colour
 * processing, so no output pixel-format field exists (unlike Pipe1/2).
 */

#define DCMIPP_P0PPCR_SWAPYUV      (1 << 0)  /* Byte swap in 32-bit word */
#define DCMIPP_P0PPCR_DBM          (1 << 16) /* Double buffer mode */

/* Pipe0 interrupt enable / status / flag-clear (DCMIPP_P0IER/P0SR/P0FCR).
 * Bit 1 is the frame-capture-complete event on Pipe0 (RM0486 39.13,
 * mirrored by CMSR2 P0FRAMEF).  TODO(RM): confirm the overrun/vsync bit
 * positions before enabling those events.
 */

#define DCMIPP_P0INT_FRAME         (1 << 1)  /* Frame capture complete */

#endif /* __ARCH_ARM_SRC_STM32N6_HARDWARE_STM32_DCMIPP_H */
