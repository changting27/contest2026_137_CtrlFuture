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
#include <syslog.h>
#include <string.h>
#include <errno.h>

#include <arch/board/board.h>

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
  if (display != 0)
    {
      return -EINVAL;
    }

  /* TODO(RM): enable the LTDC clock in RCC, program the panel timing
   * (HSYNC/VSYNC/porches), configure layer 0 as a full-screen RGB565
   * plane pointing at LTDC_FB_ADDR, and enable the controller.
   */

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
  if (display != 0)
    {
      return;
    }

  /* TODO(RM): disable the LTDC layers and controller, and gate the
   * LTDC clock in RCC.
   */
}
