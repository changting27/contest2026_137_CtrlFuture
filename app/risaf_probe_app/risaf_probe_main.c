/****************************************************************************
 * packages/demos/contest2026_137_risaf_probe_app/risaf_probe_main.c
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

#include <stdio.h>
#include <errno.h>

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Implemented by the STM32N6 RISAF probe (arch layer).  Declared here
 * because the arch source headers are not on the application include path.
 */

extern int stm32n6_risaf_probe(void);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: main
 *
 * Description:
 *   RISAF firewall write-access probe (ADR-039).  Tests whether NuttX
 *   firmware can write RISAF configuration registers in DEV boot by writing
 *   and reading back a CIDCFGR whitelist pattern on a disabled RISAF region
 *   (no access enforcement is altered), then restoring it.  The arch helper
 *   logs the full before/after register state.  A write that sticks means
 *   ADR-029/032 DMA-to-SRAM could be unblocked from firmware without
 *   flash-boot; a RAZ/WI write means the custom-FSBL path of ADR-039 is the
 *   only route.
 *
 ****************************************************************************/

int main(int argc, char *argv[])
{
  int ret = stm32n6_risaf_probe();

  if (ret == 0)
    {
      printf("RISAF PROBE WRITABLE (firmware can program RISAF in DEV boot; "
             "ADR-029/032 unblockable without flash-boot)\n");
      return 0;
    }

  if (ret == -ENOTSUP)
    {
      printf("RISAF PROBE PARTIAL (CIDCFGR writable but only implemented "
             "CIDs stick; live SRAM region untouched, safe DMA unlock still "
             "needs FSBL per ADR-039; see risaf: log lines)\n");
      return 0;
    }

  printf("RISAF PROBE LOCKED (ret=%d; RISAF config locked to higher "
         "privilege, needs FSBL per ADR-039; see risaf: log lines)\n", ret);
  return 1;
}
