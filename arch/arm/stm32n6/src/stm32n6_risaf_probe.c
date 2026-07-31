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
#include <stdbool.h>
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

#ifdef CONFIG_STM32_RISAF_EARLY_PROBE

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/* Early boot runs before nx_start(), so syslog() is not available.  These
 * tiny helpers print directly on the console via arm_lowputc(), which the
 * preceding stm32n6_lowsetup() has already made usable.
 */

static void risaf_early_puts(const char *s)
{
  while (*s != '\0')
    {
      arm_lowputc(*s++);
    }
}

static void risaf_early_hex(uint32_t v)
{
  static const char hexchars[] = "0123456789abcdef";
  int i;

  for (i = 28; i >= 0; i -= 4)
    {
      arm_lowputc(hexchars[(v >> i) & 0xf]);
    }
}

/* Print a two-digit decimal (region index 0..14). */

static void risaf_early_dec2(int v)
{
  arm_lowputc((char)('0' + (v / 10) % 10));
  arm_lowputc((char)('0' + v % 10));
}

/* Print " <tag>=0x........" */

static void risaf_early_field(const char *tag, uint32_t v)
{
  arm_lowputc(' ');
  risaf_early_puts(tag);
  risaf_early_puts("=0x");
  risaf_early_hex(v);
}

/* Heuristic "does this region look like it covers the firmware SRAM"
 * flag for the human reading the scan.  NOTE: STARTR/ENDR are byte offsets
 * relative to each instance's protected-space base, not absolute addresses,
 * so this raw compare is only meaningful for an instance whose base is 0
 * (or when reasoning by eye).  It is retained purely as a scan hint and is
 * never used for enforcement; every scanned region on this board reads
 * BREN=0, so it currently flags nothing.
 */

static bool risaf_covers_sram(uint32_t start, uint32_t end)
{
  return start <= STM32_RISAF_PROBE_SRAM_ADDR &&
         STM32_RISAF_PROBE_SRAM_ADDR <= end;
}

/* Every RISAF instance this chip implements (no 10, 16..20), as parallel
 * name/offset arrays.  Scanning all of them locates which instance/region
 * governs the running SRAM -- the 2/21/22 guess in the first probe pass was
 * wrong (all empty).  Offsets are from STM32_AHB3PERIPH_BASE_S.
 */

static const char * const g_risaf_name[] =
{
  "R1 ", "R2 ", "R3 ", "R4 ", "R5 ", "R6 ", "R7 ", "R8 ", "R9 ",
  "R11", "R12", "R13", "R14", "R15", "R21", "R22", "R23"
};

static const uint32_t g_risaf_off[] =
{
  0x6000, 0x7000, 0x8000, 0x9000, 0xa000, 0xb000, 0xc000, 0xd000, 0xe000,
  0x10000, 0x11000, 0x12000, 0x13000, 0x14000, 0x15000, 0x16000, 0x17000
};

#define RISAF_INST_COUNT \
  (sizeof(g_risaf_off) / sizeof(g_risaf_off[0]))

/* Dump one RISAF instance: its CR + illegal-access latch (IASR/IAESR/IADDR,
 * which records the CID and address of any transaction the firewall has
 * already rejected -- e.g. a DMA write), then every enabled base region and
 * enabled subregion, flagging whichever covers the firmware SRAM.
 * Read-only: every register here is CPU-readable (secure/priv/CID1 = TDCID);
 * reading changes no enforcement.  Only instances with an enabled region or
 * latched illegal access print detail, to keep early-boot output short.
 */

static void risaf_dump_instance(const char *name, uint32_t base)
{
  uint32_t cr   = getreg32(base + STM32_RISAF_CR_OFFSET);
  uint32_t iasr = getreg32(base + STM32_RISAF_IASR_OFFSET);
  bool     any  = false;
  int      n;

  /* Illegal-access latch: if the firewall has rejected a transaction, this
   * names the offending CID and address -- the direct answer to "who was
   * blocked writing where".
   */

  if ((iasr & (RISAF_IASR_IAEF | RISAF_IASR_CAEF)) != 0)
    {
      risaf_early_puts("risaf-early: ");
      risaf_early_puts(name);
      risaf_early_puts(" ILLEGAL");
      risaf_early_field("IASR", iasr);
      risaf_early_field("IAESR",
                        getreg32(base + STM32_RISAF_IAESR_OFFSET));
      risaf_early_field("IADDR",
                        getreg32(base + STM32_RISAF_IADDR_OFFSET));
      risaf_early_puts("\r\n");
    }

  for (n = 0; n < STM32_RISAF_REG_COUNT; n++)
    {
      uint32_t rb    = STM32_RISAF_REG(base, n);
      uint32_t cfgr  = getreg32(rb + STM32_RISAF_REG_CFGR);
      uint32_t start = getreg32(rb + STM32_RISAF_REG_STARTR);
      uint32_t end   = getreg32(rb + STM32_RISAF_REG_ENDR);
      uint32_t acfgr = getreg32(rb + STM32_RISAF_REG_ACFGR);
      uint32_t bcfgr = getreg32(rb + STM32_RISAF_REG_BCFGR);

      if ((cfgr & RISAF_REG_CFGR_BREN) == 0)
        {
          continue;
        }

      any = true;
      risaf_early_puts("risaf-early:   ");
      risaf_early_puts(name);
      arm_lowputc(' ');
      risaf_early_puts("R");
      risaf_early_dec2(n);
      risaf_early_field("CFG", cfgr);
      risaf_early_field("ST", start);
      risaf_early_field("EN", end);
      risaf_early_field("CID", getreg32(rb + STM32_RISAF_REG_CIDCFGR));
      if (risaf_covers_sram(start, end))
        {
          risaf_early_puts(" <=SRAM");
        }

      risaf_early_puts("\r\n");

      /* Subregion A / B, only if enabled (SREN). */

      if ((acfgr & RISAF_REG_SUBCFGR_SREN) != 0)
        {
          uint32_t as = getreg32(rb + STM32_RISAF_REG_ASTARTR);
          uint32_t ae = getreg32(rb + STM32_RISAF_REG_AENDR);

          risaf_early_puts("risaf-early:     subA");
          risaf_early_field("CFG", acfgr);
          risaf_early_field("ST", as);
          risaf_early_field("EN", ae);
          if (risaf_covers_sram(as, ae))
            {
              risaf_early_puts(" <=SRAM");
            }

          risaf_early_puts("\r\n");
        }

      if ((bcfgr & RISAF_REG_SUBCFGR_SREN) != 0)
        {
          uint32_t bs = getreg32(rb + STM32_RISAF_REG_BSTARTR);
          uint32_t be = getreg32(rb + STM32_RISAF_REG_BENDR);

          risaf_early_puts("risaf-early:     subB");
          risaf_early_field("CFG", bcfgr);
          risaf_early_field("ST", bs);
          risaf_early_field("EN", be);
          if (risaf_covers_sram(bs, be))
            {
              risaf_early_puts(" <=SRAM");
            }

          risaf_early_puts("\r\n");
        }
    }

  /* One-line summary for instances with neither an enabled region nor a
   * latched illegal access, so an all-empty scan is still evident.
   */

  if (!any && (iasr & (RISAF_IASR_IAEF | RISAF_IASR_CAEF)) == 0)
    {
      risaf_early_puts("risaf-early: ");
      risaf_early_puts(name);
      risaf_early_field("CR", cr);
      risaf_early_puts(" (no enabled region)\r\n");
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: stm32n6_risaf_early_probe
 *
 * Description:
 *   Run the RISAF investigation in the earliest safe boot window: after
 *   stm32n6_lowsetup() (console up) but before nx_start() (heap not yet
 *   initialised, memory layout still "virgin").  This is the timing ADR-039
 *   argues a real DMA->SRAM unlock should use, so the probe measures the
 *   facts that timing depends on, without changing any enforcement:
 *
 *     1. The ACTIVE region map across ALL RISAF instances, with the region
 *        that covers the running firmware SRAM (0x34000400) flagged
 *        "<=SRAM".  The first pass guessed RISAF2/21/22 and found them
 *        empty, so this scan finds the real owner -- needed to tell the
 *        CPU-SRAM region (must NOT be retouched) from free address space.
 *     2. Any latched illegal-access record (IASR/IAESR/IADDR) -- if the
 *        firewall already rejected a DMA write, this names the CID+address.
 *     3. That CIDCFGR is still firmware-writable this early (re-run the
 *        disabled-scratch-region write/readback/restore on RISAF21 REG[0]).
 *
 *   Enables no region, sets no GLOCK, leaves every register bit-identical to
 *   boot.  Purely observational: proves feasibility, does not perform the
 *   unlock (that belongs in the dedicated DMA sub-topic, not this probe).
 *
 ****************************************************************************/

void stm32n6_risaf_early_probe(void)
{
  uint32_t regbase;
  uint32_t cfgr;
  uint32_t cr;
  uint32_t before;
  uint32_t after;
  unsigned int i;

  /* Bring up the RIFSC/RISAF clock (atomic read-modify-write). */

  modifyreg32(STM32_RCC_AHB3ENR, 0, RCC_AHB3ENR_RIFSCEN);

  risaf_early_puts("risaf-early: == RISAF scan (all instances) ==\r\n");
  for (i = 0; i < RISAF_INST_COUNT; i++)
    {
      risaf_dump_instance(g_risaf_name[i],
                          STM32_RISAF_INST_OFFSET(g_risaf_off[i]));
    }

  /* Re-confirm CIDCFGR writability this early, on the disabled REG[0]
   * scratch region (same safety gate as the app probe: only if BREN=0 and
   * the container is not globally locked; write, read back, restore).
   */

  regbase = STM32_RISAF_REG(STM32_RISAF21_BASE, 0);
  cr      = getreg32(STM32_RISAF21_BASE + STM32_RISAF_CR_OFFSET);
  cfgr    = getreg32(regbase + STM32_RISAF_REG_CFGR);

  if ((cfgr & RISAF_REG_CFGR_BREN) != 0 || (cr & RISAF_CR_GLOCK) != 0)
    {
      risaf_early_puts("risaf-early: REG0 unsafe (BREN/GLOCK) skip\r\n");
      return;
    }

  before = getreg32(regbase + STM32_RISAF_REG_CIDCFGR);
  putreg32(RISAF_PROBE_CIDCFGR_PATTERN, regbase + STM32_RISAF_REG_CIDCFGR);
  after  = getreg32(regbase + STM32_RISAF_REG_CIDCFGR);
  putreg32(before, regbase + STM32_RISAF_REG_CIDCFGR);

  risaf_early_puts("risaf-early: REG0 before=0x");
  risaf_early_hex(before);
  risaf_early_puts(" wrote=0x");
  risaf_early_hex(RISAF_PROBE_CIDCFGR_PATTERN);
  risaf_early_puts(" readback=0x");
  risaf_early_hex(after);
  risaf_early_puts("\r\n");

  if (after == RISAF_PROBE_CIDCFGR_PATTERN)
    {
      risaf_early_puts("risaf-early: EARLY WRITABLE\r\n");
    }
  else if ((after & RISAF_PROBE_CIDCFGR_PATTERN) != 0)
    {
      risaf_early_puts("risaf-early: EARLY PARTIAL\r\n");
    }
  else
    {
      risaf_early_puts("risaf-early: EARLY LOCKED\r\n");
    }
}

#endif /* CONFIG_STM32_RISAF_EARLY_PROBE */
