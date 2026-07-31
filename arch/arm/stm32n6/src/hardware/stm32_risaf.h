/****************************************************************************
 * arch/arm/stm32n6/src/hardware/stm32_risaf.h
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

#ifndef __ARCH_ARM_SRC_STM32N6_HARDWARE_STM32_RISAF_H
#define __ARCH_ARM_SRC_STM32N6_HARDWARE_STM32_RISAF_H

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* RISAF (RIF Security Attribute Firewall) — address-space access control.
 *
 * Each RISAF instance governs a memory area.  A bus transaction hitting an
 * enabled region is admitted only if the initiator's compartment ID (CID)
 * is on that region's read/write whitelist (REG[n].CIDCFGR).  RISAF21 (AHB
 * RAM 0) governs part of the AXI SRAM; on this DEV-boot board its REG[0] is
 * disabled (BREN=0) and the container is not globally locked (CR.GLOCK=0),
 * so REG[0] is a safe scratch region for probing whether firmware can write
 * RISAF config at all — writing a disabled region changes no enforcement.
 *
 * The DEV-boot CPU runs in the secure world, so the secure alias must be
 * used (the non-secure alias RIFSC_BASE reads 0 for these registers).
 * RISAF21_BASE_S = PERIPH_BASE_S(0x50000000) + AHB3(0x04020000) + 0x015000.
 */

#define STM32_RISAF21_BASE          0x54035000ul

#define STM32_RISAF_CR_OFFSET       0x0000
#  define RISAF_CR_GLOCK            (1 << 0)   /* Global lock (frozen) */

#define STM32_RISAF_REG_BASE_OFFSET 0x0040
#define STM32_RISAF_REG_STRIDE      0x0040

#define STM32_RISAF_REG_CFGR        0x0000
#  define RISAF_REG_CFGR_BREN       (1 << 0)   /* Base region enable */
#  define RISAF_REG_CFGR_SEC        (1 << 8)   /* Region secure */
#define STM32_RISAF_REG_STARTR      0x0004
#define STM32_RISAF_REG_ENDR        0x0008
#define STM32_RISAF_REG_CIDCFGR     0x000c
#  define RISAF_REG_CIDCFGR_RDEN_SHIFT  0      /* Bits 0-7: read whitelist */
#  define RISAF_REG_CIDCFGR_WREN_SHIFT  16     /* Bits 16-23: write list */

#define STM32_RISAF_REG(base, n) \
  ((base) + STM32_RISAF_REG_BASE_OFFSET + ((n) * STM32_RISAF_REG_STRIDE))

/* CIDCFGR test pattern: read-enable CID 0..7 (bits 0-7) plus write-enable
 * CID 0..7 (bits 16-23).  All bits are defined (no reserved/aligned fields),
 * so a faithful read-back proves the write stuck.
 */

#define RISAF_PROBE_CIDCFGR_PATTERN 0x00ff00fful

#endif /* __ARCH_ARM_SRC_STM32N6_HARDWARE_STM32_RISAF_H */
