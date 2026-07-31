/****************************************************************************
 * packages/demos/contest2026_137_dma2d_unlock_app/dma2d_unlock_main.c
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

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Implemented by the STM32N6 DMA2D driver (arch layer).  Declared here
 * because the arch source headers are not on the application include path.
 */

extern int stm32n6_dma2d_unlock_probe(void);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: main
 *
 * Description:
 *   RISAF-unlock experiment (ADR-039 DEV-boot sub-topic).  Enables an
 *   explicit RISAF2 region whitelisting CID0 (DMA2D) and CID1 (CPU), then
 *   sweeps the DMA2D master CID running an R2M SRAM fill.  PASSes iff the
 *   CID0 write lands under the enabled region, proving a NuttX-programmed
 *   RISAF region unblocks DMA-to-SRAM in DEV boot without a custom FSBL.
 *   Per-CID land/drop detail is logged by the arch helper.
 *
 ****************************************************************************/

int main(int argc, char *argv[])
{
  int ret = stm32n6_dma2d_unlock_probe();

  if (ret == 0)
    {
      printf("DMA2D UNLOCK PASS (enabled RISAF2 region let DMA2D "
             "write SRAM)\n");
      return 0;
    }

  printf("DMA2D UNLOCK FAIL (ret=%d, see dma2d-unlock: log lines above)\n",
         ret);
  return 1;
}
