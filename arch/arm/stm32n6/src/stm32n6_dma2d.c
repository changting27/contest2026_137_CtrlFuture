/****************************************************************************
 * arch/arm/src/stm32n6/stm32n6_dma2d.c
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

/* ADR-032: DMA2D de-risk probe.
 *
 * Before committing to a full DMA2D driver this helper answers one question
 * that only real silicon can settle: in DEV boot, can DMA2D actually land a
 * write in SRAM, or is it firewalled the way GPDMA1 is?  GPDMA1 cannot be
 * unblocked because it is not a RIF-configurable master; DMA2D *is* (master
 * index 8), so its compartment ID (CID) can be reprogrammed via RIFSC.
 *
 * The probe: read the RISAF7 region that covers firmware SRAM to learn which
 * CID it grants write access, present that CID from DMA2D via RIMC, then run
 * a register-to-memory (R2M) fill of a known word into a cache-line-aligned
 * SRAM buffer and read it back.  A sentinel pre-fill (cleaned to physical
 * SRAM) plus a post-fill D-cache invalidate distinguishes a real DMA2D write
 * from a silently dropped (RAZ/WI) firewalled write -- the same technique
 * that exposed the GPDMA1 block.  Everything is logged so a single real-HW
 * run yields the full diagnosis whether it passes or fails.
 */

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>
#include <errno.h>
#include <syslog.h>

#include <nuttx/arch.h>
#include <nuttx/cache.h>

#include "arm_internal.h"
#include "hardware/stm32_dma2d.h"
#include "hardware/stm32_rcc.h"
#ifdef CONFIG_STM32_DMA2D_UNLOCK
#  include "hardware/stm32_risaf.h"
#endif

#ifdef CONFIG_STM32_DMA2D

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Cortex-M55 D-cache line size (ADR-007 keeps D-cache enabled). */

#define DMA2D_DCACHE_LINE   32

/* R2M probe geometry: one line of 8 ARGB8888 pixels == 32 bytes == one
 * whole D-cache line, so the invalidate touches only this buffer.
 */

#define DMA2D_PROBE_PIXELS  8

/* Distinctive markers: the sentinel is what the CPU writes first; the fill
 * value is what DMA2D should overwrite it with.  Neither is 0, so a dropped
 * write (leaving the sentinel) and a zeroed region are both distinguishable
 * from success.
 */

#define DMA2D_PROBE_SENTINEL 0x11111111u
#define DMA2D_PROBE_FILL     0xdeadbeefu

/* Bounded spin: the R2M of 8 pixels completes in a handful of cycles; this
 * keeps a stuck flag from hanging the caller.
 */

#define DMA2D_POLL_LIMIT    1000000

/* RISAF7 covers the FLEXMEM extension where the DEV-boot firmware (and this
 * buffer) live.  Region 0 is the base region for that window.
 */

#define DMA2D_PROBE_RISAF_BASE   STM32_RISAF7_BASE
#define DMA2D_PROBE_RISAF_REGION 0

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* R2M output buffer: one D-cache line, aligned so its clean/invalidate
 * touches only these 32 bytes.
 */

static uint32_t g_dma2d_probe_buf[DMA2D_PROBE_PIXELS]
  aligned_data(DMA2D_DCACHE_LINE);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: dma2d_r2m_try
 *
 * Description:
 *   Present the given RIMC master-attribute value to DMA2D, then run one
 *   register-to-memory fill of the known word into the SRAM buffer and
 *   check whether it landed.  A sentinel pre-fill (cleaned to physical
 *   SRAM) plus a post-fill D-cache invalidate tells a real write from a
 *   dropped (RAZ/WI) firewalled write.
 *
 * Returned Value:
 *   OK if the fill word landed in SRAM; -ETIMEDOUT if DMA2D never completed,
 *   -EIO on transfer/config error, -EFAULT if the write was dropped.
 *
 ****************************************************************************/

static int dma2d_r2m_try(uint32_t rimc_attr)
{
  uint32_t isr;
  uint32_t count;
  int      i;

  /* Present the requested compartment ID / attributes for DMA2D. */

  putreg32(rimc_attr, STM32_RIFSC_RIMC_ATTR(RIF_MASTER_INDEX_DMA2D));

  /* Pre-fill with the sentinel and push it to physical SRAM so a dropped
   * DMA2D write leaves the sentinel visible after invalidate.
   */

  for (i = 0; i < DMA2D_PROBE_PIXELS; i++)
    {
      g_dma2d_probe_buf[i] = DMA2D_PROBE_SENTINEL;
    }

  up_clean_dcache((uintptr_t)g_dma2d_probe_buf,
                  (uintptr_t)g_dma2d_probe_buf + sizeof(g_dma2d_probe_buf));

  /* Clear any stale DMA2D flags. */

  putreg32(DMA2D_IFCR_CTEIF | DMA2D_IFCR_CTCIF | DMA2D_IFCR_CCEIF,
           STM32_DMA2D_IFCR);

  /* Configure the R2M fill: ARGB8888 output, the known fill colour, the SRAM
   * destination, no inter-line gap, one line of DMA2D_PROBE_PIXELS pixels.
   */

  putreg32(DMA2D_OPFCCR_CM_ARGB8888, STM32_DMA2D_OPFCCR);
  putreg32(DMA2D_PROBE_FILL, STM32_DMA2D_OCOLR);
  putreg32((uint32_t)(uintptr_t)g_dma2d_probe_buf, STM32_DMA2D_OMAR);
  putreg32(0, STM32_DMA2D_OOR);
  putreg32((DMA2D_PROBE_PIXELS << DMA2D_NLR_PL_SHIFT) |
           (1 << DMA2D_NLR_NL_SHIFT), STM32_DMA2D_NLR);

  /* Launch: register-to-memory mode + START. */

  putreg32(DMA2D_CR_MODE_R2M | DMA2D_CR_START, STM32_DMA2D_CR);

  /* Wait for transfer-complete, transfer-error, or config-error. */

  count = 0;
  do
    {
      isr = getreg32(STM32_DMA2D_ISR);
      if (++count > DMA2D_POLL_LIMIT)
        {
          return -ETIMEDOUT;
        }
    }
  while ((isr & (DMA2D_ISR_TCIF | DMA2D_ISR_TEIF | DMA2D_ISR_CEIF)) == 0);

  if ((isr & (DMA2D_ISR_TEIF | DMA2D_ISR_CEIF)) != 0)
    {
      return -EIO;
    }

  /* Invalidate so the read-back comes from physical SRAM, not the cached
   * sentinel, then verify every word was overwritten with the fill value.
   */

  up_invalidate_dcache((uintptr_t)g_dma2d_probe_buf,
                       (uintptr_t)g_dma2d_probe_buf +
                       sizeof(g_dma2d_probe_buf));

  for (i = 0; i < DMA2D_PROBE_PIXELS; i++)
    {
      if (g_dma2d_probe_buf[i] != DMA2D_PROBE_FILL)
        {
          return -EFAULT;
        }
    }

  return OK;
}

/****************************************************************************
 * Name: stm32n6_dma2d_probe
 *
 * Description:
 *   DMA2D register-to-memory write probe.  DMA2D is a RIF-configurable
 *   master (index 8) whose compartment ID (CID) can be reprogrammed via
 *   RIFSC, so a firewalled SRAM write is a CID mismatch rather than the
 *   architectural dead-end that blocks GPDMA1.  A first run with CID 0 was
 *   dropped (RAZ/WI: TCIF set, no error, sentinel intact), so this sweeps
 *   every CID 0..7 with secure+privileged attributes and reports which (if
 *   any) lets the write land.  A single real-HW run therefore settles
 *   whether ADR-032 is viable (some CID works) or PARTIAL (none do).
 *
 * Returned Value:
 *   OK if some CID let DMA2D write SRAM; -EFAULT if every CID was
 *   firewalled; another negated errno on a hardware fault during the sweep.
 *
 ****************************************************************************/

int stm32n6_dma2d_probe(void)
{
  uint32_t rimc_before;
  uint32_t attr;
  int      ret;
  int      cid;

  /* Bring up the RIFSC and DMA2D clocks (atomic read-modify-write). */

  modifyreg32(STM32_RCC_AHB3ENR, 0, RCC_AHB3ENR_RIFSCEN);
  modifyreg32(STM32_RCC_AHB5ENR, 0, RCC_AHB5ENR_DMA2DEN);

  rimc_before = getreg32(STM32_RIFSC_RIMC_ATTR(RIF_MASTER_INDEX_DMA2D));
  syslog(LOG_INFO, "dma2d: RIMC_ATTR[8] before=0x%08lx, sweeping CID 0..7 "
         "(secure+priv)\n", (unsigned long)rimc_before);

  /* Sweep every compartment ID with secure+privileged attributes (matching a
   * Secure-only boot).  The first CID whose write lands identifies the CID
   * the firmware-SRAM RISAF region trusts.
   */

  for (cid = 0; cid <= 7; cid++)
    {
      attr = RIFSC_RIMC_ATTR_MCID(cid) | RIFSC_RIMC_ATTR_MSEC |
             RIFSC_RIMC_ATTR_MPRIV;
      ret = dma2d_r2m_try(attr);

      syslog(LOG_INFO, "dma2d: CID %d -> %s\n", cid,
             ret == OK        ? "write landed" :
             ret == -EFAULT   ? "dropped (firewalled)" :
             ret == -ETIMEDOUT ? "timeout" : "xfer error");

      if (ret == OK)
        {
          syslog(LOG_INFO,
                 "dma2d: CID %d grants DMA2D write to SRAM\n", cid);
          return OK;
        }
    }

  syslog(LOG_ERR, "dma2d: no CID 0..7 let DMA2D write SRAM (firewalled)\n");
  return -EFAULT;
}

#ifdef CONFIG_STM32_DMA2D_UNLOCK

/****************************************************************************
 * Name: stm32n6_dma2d_unlock_probe
 *
 * Description:
 *   RISAF-unlock experiment.  The plain CID sweep (stm32n6_dma2d_probe)
 *   found that with NO enabled RISAF region anywhere, DMA2D writes to SRAM
 *   are dropped for every presented CID -- i.e. the block is the RISAF
 *   *default policy*, not a region whitelist that excludes the DMA master.
 *   This probe tests the fix directly: enable an explicit RISAF2 region
 *   (RISAF2 governs AXISRAM0 @ 0x34000000, where the firmware and probe
 *   buffer live) that whitelists CID0 (DMA2D) and CID1 (CPU/TDCID), then
 *   sweep the DMA2D master CID 0..7 and see which land.
 *
 *   Expected on success: CID0 and CID1 LAND (the whitelist admits them),
 *   CID2..7 stay DROPPED (proving the whitelist is actually enforced as
 *   configured, not a blanket allow).  That is decisive evidence that a
 *   NuttX-programmed RISAF region unblocks DMA-to-SRAM without an FSBL.
 *
 *   Safety: the region is programmed while DISABLED (bounds + whitelist
 *   first, BREN last), the whitelist ALWAYS includes CID1 so the CPU can
 *   never be locked out of its own SRAM, and every touched field is
 *   restored bit-identical to boot (BREN cleared first).  STARTR/ENDR are
 *   byte offsets RELATIVE to STM32_RISAF2_SPACE_BASE (not absolute), 4 KiB
 *   granularity, both bounds inclusive.  DEV boot makes any misstep
 *   reset-recoverable (RISAF config is volatile).
 *
 * Returned Value:
 *   OK if the CID0 write landed under the enabled region; -EFAULT if it was
 *   still dropped; -EBUSY if REG[0] was already enabled or the container was
 *   locked (aborts without touching hardware); another negated errno on a
 *   transfer fault.
 *
 ****************************************************************************/

int stm32n6_dma2d_unlock_probe(void)
{
  uint32_t rb;
  uint32_t buf_off;
  uint32_t rstart;
  uint32_t rend;
  uint32_t cfgr_before;
  uint32_t start_before;
  uint32_t end_before;
  uint32_t cid_before;
  uint32_t attr;
  uint32_t rimc_rb;
  int      cid0_ret = -EIO;
  int      ret;
  int      cid;

  /* Bring up the RIFSC and DMA2D clocks (atomic read-modify-write). */

  modifyreg32(STM32_RCC_AHB3ENR, 0, RCC_AHB3ENR_RIFSCEN);
  modifyreg32(STM32_RCC_AHB5ENR, 0, RCC_AHB5ENR_DMA2DEN);

  /* Target RISAF2 REG[0] -- RISAF2 governs the AXI SRAM (0x34000000) the
   * firmware and probe buffer live in.
   */

  rb = STM32_RISAF_REG(STM32_RISAF2_BASE, 0);

  /* Safety gate: only proceed if REG[0] is disabled and the container is
   * not globally locked; otherwise abort without touching any register.
   */

  cfgr_before = getreg32(rb + STM32_RISAF_REG_CFGR);
  if ((cfgr_before & RISAF_REG_CFGR_BREN) != 0 ||
      (getreg32(STM32_RISAF2_BASE + STM32_RISAF_CR_OFFSET) &
       RISAF_CR_GLOCK) != 0)
    {
      syslog(LOG_ERR, "dma2d-unlock: RISAF2 REG0 enabled/locked, abort\n");
      return -EBUSY;
    }

  /* Region bounds as OFFSETS relative to the RISAF2 protected-space base,
   * 4 KiB-aligned to cover the whole probe buffer (both bounds inclusive).
   */

  buf_off = (uint32_t)((uintptr_t)g_dma2d_probe_buf -
                       STM32_RISAF2_SPACE_BASE);
  rstart  = buf_off & ~(STM32_RISAF2_GRANULARITY - 1);
  rend    = (buf_off + sizeof(g_dma2d_probe_buf) - 1) |
            (STM32_RISAF2_GRANULARITY - 1);

  /* Save the fields we will change so we can restore them bit-identically. */

  start_before = getreg32(rb + STM32_RISAF_REG_STARTR);
  end_before   = getreg32(rb + STM32_RISAF_REG_ENDR);
  cid_before   = getreg32(rb + STM32_RISAF_REG_CIDCFGR);

  syslog(LOG_INFO,
         "dma2d-unlock: RISAF2 REG0 start_off=0x%05lx end_off=0x%05lx "
         "buf=%p, whitelist CID0|CID1, sweeping DMA2D CID 0..7\n",
         (unsigned long)rstart, (unsigned long)rend, g_dma2d_probe_buf);

  /* Program bounds + whitelist while the region is still DISABLED, then
   * enable LAST.  This guarantees there is no window in which an enabled
   * region carries a whitelist missing CID1 (which would lock the CPU out
   * of its own SRAM).  The whitelist always includes CID1.
   */

  putreg32(rstart, rb + STM32_RISAF_REG_STARTR);
  putreg32(rend, rb + STM32_RISAF_REG_ENDR);
  putreg32(RISAF_CIDCFGR_RW(RISAF_CIDMASK_CID0 | RISAF_CIDMASK_CID1),
           rb + STM32_RISAF_REG_CIDCFGR);
  putreg32(RISAF_REG_CFGR_BREN | RISAF_REG_CFGR_SEC,
           rb + STM32_RISAF_REG_CFGR);

  /* Read the region registers back.  This is the decisive disambiguation:
   * if CFGR.BREN reads 0 the region-enable was RAZ/WI (RISAF2's active SRAM
   * container refuses a NuttX-added region), so a subsequent drop is because
   * NO region ever took effect -- not proof of a master-side gate.  If BREN
   * and the bounds/whitelist all read back as written, the region IS live
   * and a drop must be master-side.
   */

  syslog(LOG_INFO,
         "dma2d-unlock: RISAF2 REG0 readback CFGR=0x%08lx (BREN=%lu) "
         "ST=0x%08lx EN=0x%08lx CID=0x%08lx\n",
         (unsigned long)getreg32(rb + STM32_RISAF_REG_CFGR),
         (unsigned long)(getreg32(rb + STM32_RISAF_REG_CFGR) &
                         RISAF_REG_CFGR_BREN),
         (unsigned long)getreg32(rb + STM32_RISAF_REG_STARTR),
         (unsigned long)getreg32(rb + STM32_RISAF_REG_ENDR),
         (unsigned long)getreg32(rb + STM32_RISAF_REG_CIDCFGR));

  /* Sweep the DMA2D master CID.  With a CID0|CID1 whitelist, CID0 and CID1
   * should land and CID2..7 should be dropped -- proving the region is
   * enforced exactly as configured.
   */

  for (cid = 0; cid <= 7; cid++)
    {
      attr = RIFSC_RIMC_ATTR_MCID(cid) | RIFSC_RIMC_ATTR_MSEC |
             RIFSC_RIMC_ATTR_MPRIV;
      ret = dma2d_r2m_try(attr);

      /* Read RIMC_ATTR[8] back (dma2d_r2m_try wrote it): if the presented
       * attribute did not stick, the DMA2D master CID override is RAZ/WI --
       * DMA2D keeps its ROM-assigned attribute and no region can ever match
       * it, which is a master-side lock independent of the region config.
       */

      rimc_rb = getreg32(STM32_RIFSC_RIMC_ATTR(RIF_MASTER_INDEX_DMA2D));

      if (cid == 0)
        {
          cid0_ret = ret;
        }

      syslog(LOG_INFO, "dma2d-unlock: CID %d wrote_attr=0x%08lx "
             "RIMC_rb=0x%08lx -> %s%s\n", cid,
             (unsigned long)attr, (unsigned long)rimc_rb,
             ret == OK        ? "write landed" :
             ret == -EFAULT   ? "dropped (firewalled)" :
             ret == -ETIMEDOUT ? "timeout" : "xfer error",
             (cid <= 1) ? " (expect land)" : " (expect drop)");
    }

  /* Disable the region and restore every field bit-identical to boot: clear
   * BREN FIRST so enforcement stops before the bounds/whitelist revert.
   */

  putreg32(cfgr_before, rb + STM32_RISAF_REG_CFGR);
  putreg32(start_before, rb + STM32_RISAF_REG_STARTR);
  putreg32(end_before, rb + STM32_RISAF_REG_ENDR);
  putreg32(cid_before, rb + STM32_RISAF_REG_CIDCFGR);

  if (cid0_ret == OK)
    {
      syslog(LOG_INFO,
             "dma2d-unlock: an enabled RISAF2 region with CID0 whitelist "
             "lets DMA2D write SRAM -- NuttX RISAF unlock viable\n");
    }
  else
    {
      syslog(LOG_ERR,
             "dma2d-unlock: DMA2D write still dropped under an enabled "
             "whitelisted region -- block is not region-side\n");
    }

  return cid0_ret;
}

#endif /* CONFIG_STM32_DMA2D_UNLOCK */

#endif /* CONFIG_STM32_DMA2D */
