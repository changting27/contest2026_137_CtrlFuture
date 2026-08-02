/****************************************************************************
 * arch/arm/src/stm32n6/stm32n6_ltdc.c
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
 * STM32N6 LTDC (LCD-TFT Display Controller) framebuffer driver.
 *
 * This is a standard NuttX framebuffer backend.  It implements the
 * framework entry points up_fbinitialize() / up_fbgetvplane() /
 * up_fbuninitialize() (drivers/video/fb.c binds them and creates
 * /dev/fb0 when the board calls fb_register()).  Applications render
 * through the /dev/fb0 device and the FBIO* ioctls; no chip-private API
 * is exposed.
 *
 * The register-level controller programming (timing, layer setup, clock)
 * is marked TODO(RM): it needs the STM32N6 reference-manual offsets (or
 * the ST HAL_LTDC middleware), which are not yet available in-tree.  The
 * framebuffer geometry and framework contract are complete so /dev/fb0
 * comes up and can be mapped once the register writes land.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/video/fb.h>
#include <stdint.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>

#include <arch/board/board.h>

#include "arm_internal.h"
#include "hardware/stm32_rcc.h"
#include "hardware/stm32_ltdc.h"
#include "stm32n6_ltdc.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Panel geometry and framebuffer come from the board definitions. */

#define LTDC_WIDTH        BOARD_LCD_WIDTH
#define LTDC_HEIGHT       BOARD_LCD_HEIGHT
#define LTDC_FB_ADDR      BOARD_LCD_BG_ADDR
#define LTDC_BPP          16
#define LTDC_FMT          FB_FMT_RGB16_565
#define LTDC_STRIDE       (LTDC_WIDTH * (LTDC_BPP / 8))
#define LTDC_FBLEN        (LTDC_STRIDE * LTDC_HEIGHT)

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int stm32n6_ltdc_getvideoinfo(FAR struct fb_vtable_s *vtable,
                                     FAR struct fb_videoinfo_s *vinfo);
static int stm32n6_ltdc_getplaneinfo(FAR struct fb_vtable_s *vtable,
                                     int planeno,
                                     FAR struct fb_planeinfo_s *pinfo);

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Single-plane RGB565 framebuffer vtable presented to the fb framework. */

static struct fb_vtable_s g_stm32n6_ltdc_vtable =
{
  .getvideoinfo = stm32n6_ltdc_getvideoinfo,
  .getplaneinfo = stm32n6_ltdc_getplaneinfo,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: stm32n6_ltdc_getvideoinfo
 ****************************************************************************/

static int stm32n6_ltdc_getvideoinfo(FAR struct fb_vtable_s *vtable,
                                     FAR struct fb_videoinfo_s *vinfo)
{
  UNUSED(vtable);

  if (vinfo == NULL)
    {
      return -EINVAL;
    }

  vinfo->fmt     = LTDC_FMT;
  vinfo->xres    = LTDC_WIDTH;
  vinfo->yres    = LTDC_HEIGHT;
  vinfo->nplanes = 1;

  return OK;
}

/****************************************************************************
 * Name: stm32n6_ltdc_getplaneinfo
 ****************************************************************************/

static int stm32n6_ltdc_getplaneinfo(FAR struct fb_vtable_s *vtable,
                                     int planeno,
                                     FAR struct fb_planeinfo_s *pinfo)
{
  UNUSED(vtable);

  if (planeno != 0 || pinfo == NULL)
    {
      return -EINVAL;
    }

  pinfo->fbmem   = (FAR void *)LTDC_FB_ADDR;
  pinfo->fblen   = LTDC_FBLEN;
  pinfo->stride  = LTDC_STRIDE;
  pinfo->display = 0;
  pinfo->bpp     = LTDC_BPP;

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_fbinitialize
 *
 * Description:
 *   Initialize the LTDC framebuffer for the specified display.
 *
 * Input Parameters:
 *   display - Display number (only display 0 is supported).
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno value on failure.
 *
 ****************************************************************************/

int up_fbinitialize(int display)
{
  uint32_t regval;

  /* Accumulated timing boundaries (LTDC uses running sums, all -1). */

  const uint32_t hsync = BOARD_LCD_HSYNC;
  const uint32_t vsync = BOARD_LCD_VSYNC;
  const uint32_t ahbp  = BOARD_LCD_HSYNC + BOARD_LCD_HBP;
  const uint32_t avbp  = BOARD_LCD_VSYNC + BOARD_LCD_VBP;
  const uint32_t aaw   = ahbp + LTDC_WIDTH;
  const uint32_t aah   = avbp + LTDC_HEIGHT;
  const uint32_t totalw = aaw + BOARD_LCD_HFP;
  const uint32_t totalh = aah + BOARD_LCD_VFP;

  if (display != 0)
    {
      return -EINVAL;
    }

  /* Enable the LTDC peripheral clock (atomic read-modify-write). */

  modifyreg32(STM32_RCC_APB5ENR, 0, RCC_APB5ENR_LTDCEN);

  /* Program the panel timing (all boundaries are -1 per RM0486). */

  putreg32(LTDC_SSCR_HSW(hsync - 1) | LTDC_SSCR_VSH(vsync - 1),
           STM32_LTDC_SSCR);
  putreg32(LTDC_BPCR_AHBP(ahbp - 1) | LTDC_BPCR_AVBP(avbp - 1),
           STM32_LTDC_BPCR);
  putreg32(LTDC_AWCR_AAW(aaw - 1) | LTDC_AWCR_AAH(aah - 1),
           STM32_LTDC_AWCR);
  putreg32(LTDC_TWCR_TOTALW(totalw - 1) | LTDC_TWCR_TOTALH(totalh - 1),
           STM32_LTDC_TWCR);

  /* Background color black; default polarities (active low sync). */

  putreg32(0, STM32_LTDC_BCCR);

  /* Configure layer 1 as a full-screen RGB565 plane.
   *
   * Window spans the active area; positions are relative to the
   * accumulated back-porch boundary.
   */

  putreg32(LTDC_LXWHPCR_WHSTPOS(ahbp) | LTDC_LXWHPCR_WHSPPOS(aaw - 1),
           STM32_LTDC_L1WHPCR);
  putreg32(LTDC_LXWVPCR_WVSTPOS(avbp) | LTDC_LXWVPCR_WVSPPOS(aah - 1),
           STM32_LTDC_L1WVPCR);

  putreg32(LTDC_LXPFCR_PF_RGB565, STM32_LTDC_L1PFCR);
  putreg32(LTDC_LXCACR_CONSTA(0xff), STM32_LTDC_L1CACR);
  putreg32(LTDC_LXBFCR_BF1_CA | LTDC_LXBFCR_BF2_CA, STM32_LTDC_L1BFCR);

  putreg32(LTDC_FB_ADDR, STM32_LTDC_L1CFBAR);

  /* Frame buffer line length (bytes + 3) and pitch (bytes). */

  putreg32(LTDC_LXCFBLR_CFBLL(LTDC_STRIDE + 3) |
           LTDC_LXCFBLR_CFBP(LTDC_STRIDE),
           STM32_LTDC_L1CFBLR);
  putreg32(LTDC_LXCFBLNR_CFBLNBR(LTDC_HEIGHT), STM32_LTDC_L1CFBLNR);

  /* Enable layer 1. */

  putreg32(LTDC_LXCR_LEN, STM32_LTDC_L1CR);

  /* Enable the controller and force an immediate shadow reload so the
   * layer configuration takes effect.
   */

  regval = getreg32(STM32_LTDC_GCR);
  regval |= LTDC_GCR_LTDCEN;
  putreg32(regval, STM32_LTDC_GCR);

  putreg32(LTDC_SRCR_IMR, STM32_LTDC_SRCR);

  syslog(LOG_INFO, "ltdc: framebuffer %ux%u RGB565 @ %p\n",
         (unsigned)LTDC_WIDTH, (unsigned)LTDC_HEIGHT,
         (FAR void *)LTDC_FB_ADDR);
  return OK;
}

/****************************************************************************
 * Name: up_fbgetvplane
 *
 * Description:
 *   Return the framebuffer vtable for the specified display / color plane.
 *
 * Input Parameters:
 *   display - Display number (only display 0 is supported).
 *   vplane  - Color plane index (only plane 0 is supported).
 *
 * Returned Value:
 *   The framebuffer vtable on success; NULL on any failure.
 *
 ****************************************************************************/

FAR struct fb_vtable_s *up_fbgetvplane(int display, int vplane)
{
  if (display != 0 || vplane != 0)
    {
      return NULL;
    }

  return &g_stm32n6_ltdc_vtable;
}

/****************************************************************************
 * Name: up_fbuninitialize
 *
 * Description:
 *   Uninitialize the framebuffer support for the specified display.
 *
 * Input Parameters:
 *   display - Display number (only display 0 is supported).
 *
 ****************************************************************************/

void up_fbuninitialize(int display)
{
  uint32_t regval;

  if (display != 0)
    {
      return;
    }

  /* Disable layer 1 and reload, then disable the controller. */

  putreg32(0, STM32_LTDC_L1CR);
  putreg32(LTDC_SRCR_IMR, STM32_LTDC_SRCR);

  regval = getreg32(STM32_LTDC_GCR);
  regval &= ~LTDC_GCR_LTDCEN;
  putreg32(regval, STM32_LTDC_GCR);

  /* Gate the LTDC peripheral clock. */

  modifyreg32(STM32_RCC_APB5ENR, RCC_APB5ENR_LTDCEN, 0);
}
