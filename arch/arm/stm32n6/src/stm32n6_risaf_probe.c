/****************************************************************************
 * arch/arm/stm32n6/src/stm32n6_risaf_probe.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>
#include <errno.h>
#include <syslog.h>

#include <nuttx/irq.h>

#include "arm_internal.h"
#include "hardware/stm32_risaf.h"
#include "hardware/stm32_rcc.h"

#ifdef CONFIG_STM32_RISAF_PROBE

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: stm32n6_risaf_probe
 *
 * Description:
 *   Probe whether NuttX firmware can write RISAF configuration registers in
 *   DEV boot.  This is the decisive experiment gating ADR-039: the DMA2D and
 *   ADC-DMA writes to SRAM are firewalled because the RISAF region governing
 *   the AXI SRAM does not whitelist the DMA masters' CIDs.  The open
 *   question is whether firmware can add those CIDs itself, or whether the
 *   RISAF config registers are themselves locked to a higher privilege
 *   (needing a custom FSBL).
 *
 *   The earlier DMA2D sweep only reprogrammed the *master* side (RIMC CID);
 *   it never touched the *region* side (RISAF CIDCFGR whitelist).  This
 *   probe tests the region side in isolation, without changing any access
 *   enforcement:
 *
 *     - Targets RISAF21 REG[0], measured on this board as disabled (BREN=0)
 *       with the container unlocked (CR.GLOCK=0).
 *     - Keeps BREN=0 throughout: a disabled region is not evaluated by the
 *       firewall, so writing its STARTR/ENDR/CIDCFGR changes NOTHING about
 *       what the CPU or any DMA master may access.  No lock-out risk.
 *     - Writes a fully-defined bit pattern (read+write enable for CID 0..7)
 *       to CIDCFGR, reads it back, then restores the original value.
 *
 *   A faithful read-back proves firmware CAN program RISAF in DEV boot ->
 *   ADR-029/032 could be unblocked WITHOUT flash-boot (just add the DMA CID
 *   to the SRAM region's write whitelist).  A RAZ/WI read-back (value stays
 *   0 / unchanged) proves the RISAF config space is locked to a higher
 *   privilege -> the custom-FSBL path of ADR-039 is the only route.
 *
 * Returned Value:
 *   OK    - CIDCFGR write stuck (firmware can program RISAF).
 *   -EPERM- CIDCFGR write did not stick (RAZ/WI; needs FSBL/secure grant).
 *   -EBUSY- REG[0] was unexpectedly enabled or the container was locked;
 *           probe aborted without writing (leaves hardware untouched).
 *
 ****************************************************************************/

int stm32n6_risaf_probe(void)
{
  uint32_t regbase;
  uint32_t cr;
  uint32_t cfgr;
  uint32_t cid_before;
  uint32_t cid_after;
  irqstate_t flags;
  int ret;

  /* Bring up the RIFSC/RISAF clock (atomic read-modify-write). */

  modifyreg32(STM32_RCC_AHB3ENR, 0, RCC_AHB3ENR_RIFSCEN);

  regbase = STM32_RISAF_REG(STM32_RISAF21_BASE, 0);

  cr   = getreg32(STM32_RISAF21_BASE + STM32_RISAF_CR_OFFSET);
  cfgr = getreg32(regbase + STM32_RISAF_REG_CFGR);

  syslog(LOG_INFO,
         "risaf: RISAF21 CR=0x%08lx (GLOCK=%lu) REG0.CFGR=0x%08lx "
         "(BREN=%lu)\n",
         (unsigned long)cr, (unsigned long)(cr & RISAF_CR_GLOCK),
         (unsigned long)cfgr,
         (unsigned long)(cfgr & RISAF_REG_CFGR_BREN));

  /* Safety gate: only proceed if REG[0] is disabled and the container is
   * not globally locked.  If either precondition fails, abort WITHOUT
   * writing so we never disturb a live region or hit a frozen register.
   */

  if ((cfgr & RISAF_REG_CFGR_BREN) != 0)
    {
      syslog(LOG_ERR, "risaf: REG0 is ENABLED (BREN=1); aborting probe "
                      "to avoid disturbing a live region\n");
      return -EBUSY;
    }

  if ((cr & RISAF_CR_GLOCK) != 0)
    {
      syslog(LOG_ERR, "risaf: container GLOCK set; RISAF frozen until "
                      "reset, firmware cannot reconfigure\n");
      return -EBUSY;
    }

  /* Write-and-read-back the CIDCFGR whitelist of the disabled region.
   * BREN stays 0 the whole time, so this alters no access enforcement.
   * Guard with a critical section so nothing interleaves write/restore.
   */

  flags = up_irq_save();

  cid_before = getreg32(regbase + STM32_RISAF_REG_CIDCFGR);
  putreg32(RISAF_PROBE_CIDCFGR_PATTERN, regbase + STM32_RISAF_REG_CIDCFGR);
  cid_after = getreg32(regbase + STM32_RISAF_REG_CIDCFGR);

  /* Restore the original value immediately (region stays disabled either
   * way; this simply leaves the register bit-identical to boot).
   */

  putreg32(cid_before, regbase + STM32_RISAF_REG_CIDCFGR);

  up_irq_restore(flags);

  syslog(LOG_INFO,
         "risaf: CIDCFGR before=0x%08lx wrote=0x%08lx readback=0x%08lx "
         "restored=0x%08lx\n",
         (unsigned long)cid_before,
         (unsigned long)RISAF_PROBE_CIDCFGR_PATTERN,
         (unsigned long)cid_after,
         (unsigned long)getreg32(regbase + STM32_RISAF_REG_CIDCFGR));

  if (cid_after == RISAF_PROBE_CIDCFGR_PATTERN)
    {
      /* Every bit stuck: firmware can freely program this register. */

      syslog(LOG_INFO, "risaf: WRITE STUCK -- firmware CAN program RISAF "
                       "in DEV boot; ADR-029/032 unblockable no flash\n");
      ret = OK;
    }
  else if ((cid_after & RISAF_PROBE_CIDCFGR_PATTERN) != 0)
    {
      /* Some bits stuck (e.g. 0x00030003 = CID0/1 read+write only): the
       * CIDCFGR register IS writable from firmware -- it is NOT locked to a
       * higher privilege -- but this RISAF instance implements only the
       * compartments whose bits stuck.  So firmware can edit whitelists, yet
       * DMA-to-SRAM stays blocked by the *active* region governing the SRAM
       * (not this disabled scratch region), which we deliberately do not
       * retouch at runtime (reconfiguring the live firmware-SRAM region
       * risks locking the CPU out).  Provisioning that region safely belongs
       * in early FSBL -- ADR-039 stands, now for a precise reason.
       */

      syslog(LOG_WARNING,
             "risaf: WRITE PARTIAL (readback=0x%08lx) -- CIDCFGR writable "
             "but only implemented CIDs stick; live SRAM region unchanged, "
             "safe DMA unlock still needs FSBL (ADR-039)\n",
             (unsigned long)cid_after);
      ret = -ENOTSUP;
    }
  else
    {
      /* Nothing stuck: register is read-as-zero / write-ignored. */

      syslog(LOG_ERR, "risaf: WRITE IGNORED (RAZ/WI) -- RISAF config "
                      "locked to higher priv; needs FSBL (ADR-039)\n");
      ret = -EPERM;
    }

  return ret;
}

#endif /* CONFIG_STM32_RISAF_PROBE */
