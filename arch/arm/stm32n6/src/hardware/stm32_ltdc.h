/****************************************************************************
 * arch/arm/src/stm32n6/hardware/stm32_ltdc.h
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

#ifndef __ARCH_ARM_SRC_STM32N6_HARDWARE_STM32_LTDC_H
#define __ARCH_ARM_SRC_STM32N6_HARDWARE_STM32_LTDC_H

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
 * NOTE: The STM32N6 LTDC is a revised version of the LCD-TFT IP used on
 * the STM32F4/F7/H7.  The global/timing block (offsets 0x008..0x048) is
 * identical to those parts, but the per-layer block was relocated and
 * uses a 0x100 stride (vs 0x80 on H7), and the pixel-format encoding
 * differs.  All values below are taken from RM0486 section 43 (LTDC
 * registers) / Table 386, NOT copied from the H7 header.
 */

/* Global registers */

#define STM32_LTDC_SSCR_OFFSET     0x0008 /* Synchronization size config */
#define STM32_LTDC_BPCR_OFFSET     0x000c /* Back porch config */
#define STM32_LTDC_AWCR_OFFSET     0x0010 /* Active width config */
#define STM32_LTDC_TWCR_OFFSET     0x0014 /* Total width config */
#define STM32_LTDC_GCR_OFFSET      0x0018 /* Global control */
#define STM32_LTDC_SRCR_OFFSET     0x0024 /* Shadow reload config */
#define STM32_LTDC_GCCR_OFFSET     0x0028 /* Gamma correction config */
#define STM32_LTDC_BCCR_OFFSET     0x002c /* Background color config */
#define STM32_LTDC_IER_OFFSET      0x0034 /* Interrupt enable */
#define STM32_LTDC_ISR_OFFSET      0x0038 /* Interrupt status */
#define STM32_LTDC_ICR_OFFSET      0x003c /* Interrupt clear */
#define STM32_LTDC_LIPCR_OFFSET    0x0040 /* Line interrupt position */
#define STM32_LTDC_CPSR_OFFSET     0x0044 /* Current position status */
#define STM32_LTDC_CDSR_OFFSET     0x0048 /* Current display status */

/* Layer 1 registers.  RM0486 lists these as "offset + 0x100 * (x - 1)"
 * for x = 1..2; the values below are for layer 1 (x = 1).  Layer 2 is
 * reached by adding STM32_LTDC_LAYER_STRIDE.
 */

#define STM32_LTDC_LAYER_STRIDE    0x0100

#define STM32_LTDC_L1CR_OFFSET     0x010c /* Layer 1 control */
#define STM32_LTDC_L1WHPCR_OFFSET  0x0110 /* Layer 1 window horiz position */
#define STM32_LTDC_L1WVPCR_OFFSET  0x0114 /* Layer 1 window vert position */
#define STM32_LTDC_L1CKCR_OFFSET   0x0118 /* Layer 1 color keying config */
#define STM32_LTDC_L1PFCR_OFFSET   0x011c /* Layer 1 pixel format config */
#define STM32_LTDC_L1CACR_OFFSET   0x0120 /* Layer 1 constant alpha config */
#define STM32_LTDC_L1DCCR_OFFSET   0x0124 /* Layer 1 default color config */
#define STM32_LTDC_L1BFCR_OFFSET   0x0128 /* Layer 1 blend factors cfg */
#define STM32_LTDC_L1CFBAR_OFFSET  0x0134 /* Layer 1 color FB address */
#define STM32_LTDC_L1CFBLR_OFFSET  0x0138 /* Layer 1 color FB length */
#define STM32_LTDC_L1CFBLNR_OFFSET 0x013c /* Layer 1 color FB line number */

/* Register addresses *******************************************************/

#define STM32_LTDC_SSCR    (STM32_LTDC_BASE + STM32_LTDC_SSCR_OFFSET)
#define STM32_LTDC_BPCR    (STM32_LTDC_BASE + STM32_LTDC_BPCR_OFFSET)
#define STM32_LTDC_AWCR    (STM32_LTDC_BASE + STM32_LTDC_AWCR_OFFSET)
#define STM32_LTDC_TWCR    (STM32_LTDC_BASE + STM32_LTDC_TWCR_OFFSET)
#define STM32_LTDC_GCR     (STM32_LTDC_BASE + STM32_LTDC_GCR_OFFSET)
#define STM32_LTDC_SRCR    (STM32_LTDC_BASE + STM32_LTDC_SRCR_OFFSET)
#define STM32_LTDC_BCCR    (STM32_LTDC_BASE + STM32_LTDC_BCCR_OFFSET)
#define STM32_LTDC_IER     (STM32_LTDC_BASE + STM32_LTDC_IER_OFFSET)
#define STM32_LTDC_ISR     (STM32_LTDC_BASE + STM32_LTDC_ISR_OFFSET)
#define STM32_LTDC_ICR     (STM32_LTDC_BASE + STM32_LTDC_ICR_OFFSET)

#define STM32_LTDC_L1CR    (STM32_LTDC_BASE + STM32_LTDC_L1CR_OFFSET)
#define STM32_LTDC_L1WHPCR (STM32_LTDC_BASE + STM32_LTDC_L1WHPCR_OFFSET)
#define STM32_LTDC_L1WVPCR (STM32_LTDC_BASE + STM32_LTDC_L1WVPCR_OFFSET)
#define STM32_LTDC_L1PFCR  (STM32_LTDC_BASE + STM32_LTDC_L1PFCR_OFFSET)
#define STM32_LTDC_L1CACR  (STM32_LTDC_BASE + STM32_LTDC_L1CACR_OFFSET)
#define STM32_LTDC_L1BFCR  (STM32_LTDC_BASE + STM32_LTDC_L1BFCR_OFFSET)
#define STM32_LTDC_L1CFBAR (STM32_LTDC_BASE + STM32_LTDC_L1CFBAR_OFFSET)
#define STM32_LTDC_L1CFBLR (STM32_LTDC_BASE + STM32_LTDC_L1CFBLR_OFFSET)
#define STM32_LTDC_L1CFBLNR (STM32_LTDC_BASE + STM32_LTDC_L1CFBLNR_OFFSET)

/* Register bit definitions *************************************************/

/* Synchronization size / back porch / active / total width config.
 * All four share the same field layout: vertical in bits 0..10,
 * horizontal in bits 16..27.
 */

#define LTDC_SSCR_VSH_SHIFT        (0)
#define LTDC_SSCR_VSH_MASK         (0x7ff << LTDC_SSCR_VSH_SHIFT)
#  define LTDC_SSCR_VSH(n)         ((uint32_t)(n) << LTDC_SSCR_VSH_SHIFT)
#define LTDC_SSCR_HSW_SHIFT        (16)
#define LTDC_SSCR_HSW_MASK         (0xfff << LTDC_SSCR_HSW_SHIFT)
#  define LTDC_SSCR_HSW(n)         ((uint32_t)(n) << LTDC_SSCR_HSW_SHIFT)

#define LTDC_BPCR_AVBP_SHIFT       (0)
#define LTDC_BPCR_AVBP_MASK        (0x7ff << LTDC_BPCR_AVBP_SHIFT)
#  define LTDC_BPCR_AVBP(n)        ((uint32_t)(n) << LTDC_BPCR_AVBP_SHIFT)
#define LTDC_BPCR_AHBP_SHIFT       (16)
#define LTDC_BPCR_AHBP_MASK        (0xfff << LTDC_BPCR_AHBP_SHIFT)
#  define LTDC_BPCR_AHBP(n)        ((uint32_t)(n) << LTDC_BPCR_AHBP_SHIFT)

#define LTDC_AWCR_AAH_SHIFT        (0)
#define LTDC_AWCR_AAH_MASK         (0x7ff << LTDC_AWCR_AAH_SHIFT)
#  define LTDC_AWCR_AAH(n)         ((uint32_t)(n) << LTDC_AWCR_AAH_SHIFT)
#define LTDC_AWCR_AAW_SHIFT        (16)
#define LTDC_AWCR_AAW_MASK         (0xfff << LTDC_AWCR_AAW_SHIFT)
#  define LTDC_AWCR_AAW(n)         ((uint32_t)(n) << LTDC_AWCR_AAW_SHIFT)

#define LTDC_TWCR_TOTALH_SHIFT     (0)
#define LTDC_TWCR_TOTALH_MASK      (0x7ff << LTDC_TWCR_TOTALH_SHIFT)
#  define LTDC_TWCR_TOTALH(n)      ((uint32_t)(n) << LTDC_TWCR_TOTALH_SHIFT)
#define LTDC_TWCR_TOTALW_SHIFT     (16)
#define LTDC_TWCR_TOTALW_MASK      (0xfff << LTDC_TWCR_TOTALW_SHIFT)
#  define LTDC_TWCR_TOTALW(n)      ((uint32_t)(n) << LTDC_TWCR_TOTALW_SHIFT)

/* Global control register */

#define LTDC_GCR_LTDCEN            (1 << 0)  /* LTDC global enable */
#define LTDC_GCR_DEN               (1 << 16) /* Dither enable */
#define LTDC_GCR_PCPOL             (1 << 28) /* Pixel clock polarity */
#define LTDC_GCR_DEPOL             (1 << 29) /* Data enable polarity */
#define LTDC_GCR_VSPOL             (1 << 30) /* Vertical sync polarity */
#define LTDC_GCR_HSPOL             (1 << 31) /* Horizontal sync polarity */

/* Shadow reload configuration register */

#define LTDC_SRCR_IMR             (1 << 0)  /* Immediate reload */
#define LTDC_SRCR_VBR             (1 << 1)  /* Vertical blanking reload */

/* Layer control register */

#define LTDC_LXCR_LEN             (1 << 0)  /* Layer enable */
#define LTDC_LXCR_COLKEN          (1 << 1)  /* Color keying enable */
#define LTDC_LXCR_CLUTEN          (1 << 4)  /* CLUT enable */

/* Layer window horizontal/vertical position config */

#define LTDC_LXWHPCR_WHSTPOS_SHIFT (0)
#define LTDC_LXWHPCR_WHSTPOS_MASK  (0xfff << LTDC_LXWHPCR_WHSTPOS_SHIFT)
#  define LTDC_LXWHPCR_WHSTPOS(n)  ((uint32_t)(n) << \
                                    LTDC_LXWHPCR_WHSTPOS_SHIFT)
#define LTDC_LXWHPCR_WHSPPOS_SHIFT (16)
#define LTDC_LXWHPCR_WHSPPOS_MASK  (0xfff << LTDC_LXWHPCR_WHSPPOS_SHIFT)
#  define LTDC_LXWHPCR_WHSPPOS(n)  ((uint32_t)(n) << \
                                    LTDC_LXWHPCR_WHSPPOS_SHIFT)

#define LTDC_LXWVPCR_WVSTPOS_SHIFT (0)
#define LTDC_LXWVPCR_WVSTPOS_MASK  (0x7ff << LTDC_LXWVPCR_WVSTPOS_SHIFT)
#  define LTDC_LXWVPCR_WVSTPOS(n)  ((uint32_t)(n) << \
                                    LTDC_LXWVPCR_WVSTPOS_SHIFT)
#define LTDC_LXWVPCR_WVSPPOS_SHIFT (16)
#define LTDC_LXWVPCR_WVSPPOS_MASK  (0x7ff << LTDC_LXWVPCR_WVSPPOS_SHIFT)
#  define LTDC_LXWVPCR_WVSPPOS(n)  ((uint32_t)(n) << \
                                    LTDC_LXWVPCR_WVSPPOS_SHIFT)

/* Layer pixel format config register.  NOTE: the STM32N6 encoding
 * differs from F4/F7/H7 -- RGB565 is 0b100 here, not 0b010.  Values
 * from RM0486 LTDC_LxPFCR PF[2:0].
 */

#define LTDC_LXPFCR_PF_SHIFT      (0)
#define LTDC_LXPFCR_PF_MASK       (0x7 << LTDC_LXPFCR_PF_SHIFT)
#  define LTDC_LXPFCR_PF_ARGB8888 (0x0 << LTDC_LXPFCR_PF_SHIFT)
#  define LTDC_LXPFCR_PF_ABGR8888 (0x1 << LTDC_LXPFCR_PF_SHIFT)
#  define LTDC_LXPFCR_PF_RGBA8888 (0x2 << LTDC_LXPFCR_PF_SHIFT)
#  define LTDC_LXPFCR_PF_RGB565   (0x4 << LTDC_LXPFCR_PF_SHIFT)
#  define LTDC_LXPFCR_PF_RGB888   (0x6 << LTDC_LXPFCR_PF_SHIFT)

/* Layer constant alpha config register */

#define LTDC_LXCACR_CONSTA_SHIFT  (0)
#define LTDC_LXCACR_CONSTA_MASK   (0xff << LTDC_LXCACR_CONSTA_SHIFT)
#  define LTDC_LXCACR_CONSTA(n)   ((uint32_t)(n) << LTDC_LXCACR_CONSTA_SHIFT)

/* Layer blending factors config register */

#define LTDC_LXBFCR_BF2_SHIFT     (0)
#define LTDC_LXBFCR_BF2_MASK      (0x7 << LTDC_LXBFCR_BF2_SHIFT)
#  define LTDC_LXBFCR_BF2_CA      (0x5 << LTDC_LXBFCR_BF2_SHIFT)
#  define LTDC_LXBFCR_BF2_PAxCA   (0x7 << LTDC_LXBFCR_BF2_SHIFT)
#define LTDC_LXBFCR_BF1_SHIFT     (8)
#define LTDC_LXBFCR_BF1_MASK      (0x7 << LTDC_LXBFCR_BF1_SHIFT)
#  define LTDC_LXBFCR_BF1_CA      (0x6 << LTDC_LXBFCR_BF1_SHIFT)
#  define LTDC_LXBFCR_BF1_PAxCA   (0x4 << LTDC_LXBFCR_BF1_SHIFT)

/* Layer color frame buffer length register */

#define LTDC_LXCFBLR_CFBLL_SHIFT  (0)
#define LTDC_LXCFBLR_CFBLL_MASK   (0x1fff << LTDC_LXCFBLR_CFBLL_SHIFT)
#  define LTDC_LXCFBLR_CFBLL(n)   ((uint32_t)(n) << LTDC_LXCFBLR_CFBLL_SHIFT)
#define LTDC_LXCFBLR_CFBP_SHIFT   (16)
#define LTDC_LXCFBLR_CFBP_MASK    (0x1fff << LTDC_LXCFBLR_CFBP_SHIFT)
#  define LTDC_LXCFBLR_CFBP(n)    ((uint32_t)(n) << LTDC_LXCFBLR_CFBP_SHIFT)

/* Layer color frame buffer line number register */

#define LTDC_LXCFBLNR_CFBLNBR_SHIFT (0)
#define LTDC_LXCFBLNR_CFBLNBR_MASK  (0x7ff << LTDC_LXCFBLNR_CFBLNBR_SHIFT)
#  define LTDC_LXCFBLNR_CFBLNBR(n)  ((uint32_t)(n) << \
                                     LTDC_LXCFBLNR_CFBLNBR_SHIFT)

#endif /* __ARCH_ARM_SRC_STM32N6_HARDWARE_STM32_LTDC_H */
