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

/* RISAF2 governs the AXI SRAM the firmware itself runs from (code/stack);
 * ROM requires the FSBL/firmware at the 0x400 offset in its region, so this
 * container must NEVER be reprogrammed at runtime (CPU lock-out risk).  It
 * is read here only to map which region covers the running SRAM.  RISAF22
 * is the second AHB-RAM firewall (AXISRAM banks), a candidate home for a
 * dedicated DMA-buffer region.  Secure aliases: AHB3PERIPH_BASE_S + off.
 */

#define STM32_RISAF2_BASE           0x54027000ul
#define STM32_RISAF22_BASE          0x54036000ul

/* RISAF2's protected address space begins at the AXI SRAM base (RM0486:
 * CPU_axiRAM0 == SRAM1_AXI @ 0x34000000).  A region's STARTR/ENDR are byte
 * offsets from THIS base, not absolute addresses.  Region granularity for
 * RISAF2 is 4 KiB, so STARTR must be 4K-aligned and (ENDR+1) 4K-aligned.
 */

#define STM32_RISAF2_SPACE_BASE     0x34000000ul
#define STM32_RISAF2_GRANULARITY    0x00001000ul

/* Secure-alias base of the AHB3 peripheral block; all RISAF instances are
 * offsets from here (PERIPH_BASE_S 0x50000000 + AHB3 0x04020000).
 */

#define STM32_AHB3PERIPH_BASE_S     0x54020000ul

/* This chip instantiates RISAF1..9, 11..15 and 21..23 (no 10, 16..20).
 * Base = AHB3PERIPH_BASE_S + offset; each spans 0x1000.  The instance that
 * governs the firmware's own SRAM at 0x34000000 was NOT one of 2/21/22
 * (all read CR=0 with no enabled region), so the early probe scans them all
 * to find which region actually covers STM32_RISAF_PROBE_SRAM_ADDR.
 */

#define STM32_RISAF_INST_OFFSET(n)  (STM32_AHB3PERIPH_BASE_S + (n))

/* A firmware SRAM address known to be in active use (idle stack / .data live
 * here in DEV boot): used to flag which scanned region, if any, governs the
 * running SRAM so it can be told apart from free address space.
 */

#define STM32_RISAF_PROBE_SRAM_ADDR 0x34000400ul

#define STM32_RISAF_CR_OFFSET       0x0000
#  define RISAF_CR_GLOCK            (1 << 0)   /* Global lock (frozen) */
#define STM32_RISAF_IASR_OFFSET     0x0008
#  define RISAF_IASR_CAEF           (1 << 0)   /* Config access error */
#  define RISAF_IASR_IAEF           (1 << 1)   /* Illegal access error */
#define STM32_RISAF_IAESR_OFFSET    0x0020     /* IAR[0]: latched CID */
#  define RISAF_IAESR_IACID_MASK    0x7        /* Bits 0-2: offending CID */
#define STM32_RISAF_IADDR_OFFSET    0x0024     /* IAR[0]: latched address */

/* Base regions per RISAF instance (RISAF_Region_TypeDef REG[15]). */

#define STM32_RISAF_REG_COUNT       15

#define STM32_RISAF_REG_BASE_OFFSET 0x0040
#define STM32_RISAF_REG_STRIDE      0x0040

#define STM32_RISAF_REG_CFGR        0x0000
#  define RISAF_REG_CFGR_BREN       (1 << 0)   /* Base region enable */
#  define RISAF_REG_CFGR_SEC        (1 << 8)   /* Region secure */

/* STARTR/ENDR are NOT absolute addresses: RM0486 defines both as a
 * byte-address offset RELATIVE to the base of the instance's protected
 * address space (e.g. for RISAF2 that base is 0x34000000).  Both bounds are
 * inclusive; START must be granularity-aligned and (END+1) too.
 */

#define STM32_RISAF_REG_STARTR      0x0004     /* Region start offset (rel) */
#define STM32_RISAF_REG_ENDR        0x0008     /* Region end offset (rel,inc)*/
#define STM32_RISAF_REG_CIDCFGR     0x000c
#  define RISAF_REG_CIDCFGR_RDEN_SHIFT  0      /* Bits 0-7: read whitelist */
#  define RISAF_REG_CIDCFGR_WREN_SHIFT  16     /* Bits 16-23: write list */

/* Build a CIDCFGR whitelist granting read+write to the given CID mask (a
 * bitmask of CIDs, bit n = CID n).  This RISAF instance implements only CID0
 * and CID1 (measured: writing 0x00FF00FF reads back 0x00030003), so callers
 * pass RISAF_CIDMASK_CPU|... using the two-CID masks below.
 */

#define RISAF_CIDMASK_CID0          (1 << 0)
#define RISAF_CIDMASK_CID1          (1 << 1)   /* CPU / TDCID -- keep always */
#define RISAF_CIDCFGR_RW(cidmask) \
  (((cidmask) << RISAF_REG_CIDCFGR_RDEN_SHIFT) | \
   ((cidmask) << RISAF_REG_CIDCFGR_WREN_SHIFT))

/* Subregion A/B share the region's stride; each has CFGR (SREN bit0 =
 * subregion enable), STARTR and ENDR at these offsets from the region base.
 */

#define STM32_RISAF_REG_ACFGR       0x0010
#define STM32_RISAF_REG_ASTARTR     0x0014
#define STM32_RISAF_REG_AENDR       0x0018
#define STM32_RISAF_REG_BCFGR       0x0020
#define STM32_RISAF_REG_BSTARTR     0x0024
#define STM32_RISAF_REG_BENDR       0x0028
#  define RISAF_REG_SUBCFGR_SREN    (1 << 0)   /* Subregion enable */

#define STM32_RISAF_REG(base, n) \
  ((base) + STM32_RISAF_REG_BASE_OFFSET + ((n) * STM32_RISAF_REG_STRIDE))

/* CIDCFGR test pattern: read-enable CID 0..7 (bits 0-7) plus write-enable
 * CID 0..7 (bits 16-23).  All bits are defined (no reserved/aligned fields),
 * so a faithful read-back proves the write stuck.
 */

#define RISAF_PROBE_CIDCFGR_PATTERN 0x00ff00fful

#endif /* __ARCH_ARM_SRC_STM32N6_HARDWARE_STM32_RISAF_H */
