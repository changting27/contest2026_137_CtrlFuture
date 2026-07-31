/****************************************************************************
 * arch/arm/src/stm32n6/stm32n6_start.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/cache.h>
#include <nuttx/init.h>

#include <stdint.h>

#include <arch/board/board.h>
#include <arch/barriers.h>

#include "arm_internal.h"
#include "nvic.h"
#include "hardware/stm32_rcc.h"
#include "hardware/stm32_pwr.h"
#include "hardware/stm32_syscfg.h"
#include "stm32n6_rcc.h"
#include "stm32n6_pwr.h"
#include "stm32n6_lowputc.h"

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

void stm32_boardinitialize(void);

#ifdef CONFIG_STM32_RISAF_EARLY_PROBE
void stm32n6_risaf_early_probe(void);
#endif

/* __start_c() carries the real boot logic; __start() below is a naked
 * dispatcher that clears the boot-ROM stack limits before any compiler
 * prologue runs.  __start_c is reached via "b __start_c" from __start's
 * inline asm and therefore must have external linkage.
 */

void __start_c(void) noinstrument_function;

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define IDLE_STACK \
  ((uintptr_t)_ebss + CONFIG_IDLETHREAD_STACKSIZE)

/****************************************************************************
 * Public Data
 ****************************************************************************/

const uintptr_t g_idle_topstack = IDLE_STACK;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: showprogress
 *
 * Description:
 *   Print a character on the UART to show boot progress.
 *
 ****************************************************************************/

#ifdef CONFIG_DEBUG_FEATURES
static inline void showprogress(char c)
{
  arm_lowputc(c);
}
#else
#  define showprogress(c)
#endif

/****************************************************************************
 * Name: stm32n6_enable_lob
 *
 * Description:
 *   Enable the Cortex-M55 ARMv8.1-M Low-Overhead Branch extension
 *   (CCR.LOB).  This gates the WLS/DLS/LE loop instructions the compiler
 *   may emit and the MVE data path.  CCR.LOB resets to 0, so it must be
 *   set before any loop the compiler could lower with LE runs.  Matches
 *   upstream stm32_start.c stm32_enable_lob().
 *
 ****************************************************************************/

static inline void stm32n6_enable_lob(void)
{
  uint32_t regval;

  regval  = getreg32(NVIC_CFGCON);
  regval |= NVIC_CFGCON_LOB;
  putreg32(regval, NVIC_CFGCON);
  UP_ISB();
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: __start
 *
 * Description:
 *   Reset entry point.  In DEV boot the STM32N6 boot ROM does not perform a
 *   Cortex-M reset sequence into our image: it *branches* to _stext with
 *   MSP still pointing at the boot ROM's own high stack region, and with
 *   MSPLIM/PSPLIM left set such that the first stack push from C code can
 *   fault.  Because MSP is never reloaded from _vectors[0] the way a real
 *   reset would, the running stack sits ~800 KiB above g_idle_topstack.
 *   arm_stack_color(idle_stack, 0) then colours everything from the idle
 *   stack base up to the *current* SP with STACK_COLOR (0xdeadbeef),
 *   overwriting the heap metadata that lives at g_idle_topstack and
 *   corrupting delay.head -> UNALIGNED UsageFault on the first malloc.
 *
 *   This function is naked so MSP can be reloaded to our idle-thread stack
 *   top and the stack limits cleared before any compiler-generated
 *   prologue runs, then it tail-calls __start_c.  On a platform that does
 *   reset normally (e.g. Renode) MSP already equals g_idle_topstack, so the
 *   reload is a harmless no-op.
 *
 ****************************************************************************/

void __attribute__((naked)) noinstrument_function __start(void)
{
  __asm__ volatile ("mov r0, #0\n\t"
                    "msr msplim, r0\n\t"
                    "msr psplim, r0\n\t"
                    "ldr r0, =g_idle_topstack\n\t"
                    "ldr r0, [r0]\n\t"
                    "msr msp, r0\n\t"
                    "isb\n\t"
                    "b __start_c\n\t");
}

/****************************************************************************
 * Name: __start_c
 *
 * Description:
 *   The C-level boot path, reached from the naked __start dispatcher.
 *
 ****************************************************************************/

void __start_c(void)
{
  const uint32_t *src;
  uint32_t *dest;

  /* The DEV-mode boot ROM leaves VTOR pointing at its own ROM region.
   * Point VTOR at our SRAM vector table before any exception path can
   * run, so an early fault is dispatched to our handlers (which decode
   * it) rather than silently into the boot ROM's vectors.  up_irqinit-
   * ialize() sets this again later; setting it here closes the window
   * from reset until NuttX runs.
   */

  putreg32((uint32_t)_vectors, NVIC_VECTAB);

  /* When chain-loaded by an FSBL that called HAL_Init(), SysTick may be
   * left running.  Disable it and clear any pending SysTick interrupt so
   * it does not fire before NuttX has attached its handler.  Matches
   * upstream stm32_start.c.
   */

  putreg32(0, NVIC_SYSTICK_CTRL);
  putreg32(NVIC_INTCTRL_PENDSTCLR, NVIC_INTCTRL);

  /* Force plain SLEEP (not DEEPSLEEP) on WFI so the system clock keeps
   * running and SysTick continues to wake us.  Cleared once here so
   * up_idle()'s WFI stays a shallow sleep on every idle entry.  (Resets
   * to 0 on this core, but clear it explicitly so the boot state does
   * not depend on a chain-loader leaving it untouched.)
   */

  modifyreg32(NVIC_SYSCON, NVIC_SYSCON_SLEEPDEEP, 0);

  /* Enable the FPU before stm32n6_clockconfig and the rest of init.  With
   * the hard-float ABI the compiler may emit FPU instructions later, and
   * any exception entry will try to push FP context -- both require
   * CP10/CP11 to be enabled.  arm_fpuconfig() is a no-op unless
   * CONFIG_ARCH_FPU is set.
   */

  arm_fpuconfig();

  /* Clear .bss */

  for (dest = (uint32_t *)_sbss; dest < (uint32_t *)_ebss; )
    {
      *dest++ = 0;
    }

  /* Copy .data from flash to SRAM.
   *
   * Ported from apache/nuttx upstream stm32_start.c: skip the copy
   * if _eronly and _sdata are the same address.  This repo's DEV
   * boot (SRAM-only, no XSPI flash, no FSBL) linker script
   * currently always places .data's load address ahead of its
   * run address, so this check is presently a no-op guard rather
   * than an active optimization -- but it is cheap, correct in
   * both cases, and protects against a future linker script
   * change (e.g. adding a flash-boot target) that made them equal
   * without this guard, which would otherwise copy .data onto
   * itself or read past a zero-length source region.
   */

  if (&_eronly[0] != &_sdata[0])
    {
      src = (const uint32_t *)_eronly;
      dest = (uint32_t *)_sdata;
      for (; dest < (uint32_t *)_edata; )
        {
          *dest++ = *src++;
        }
    }

  /* Enable the Cortex-M55 Low-Overhead-Branch extension before any code
   * that the compiler may have lowered with LE/WLS/DLS runs.
   */

  stm32n6_enable_lob();

  /* Configure clocks */

  stm32n6_clockconfig();

  /* Per ES0620, BSECEN must remain set or WFI/sleep fails.  Set it via
   * the atomic APB4ENSR2 set alias, together with SYSCFGEN which the
   * SYSCFG-based erratum mitigations would need.
   */

  putreg32(RCC_APB4ENR2_BSECEN | RCC_APB4ENR2_SYSCFGEN,
           STM32_RCC_APB4ENSR2);

  /* Enable the LPEN bits that keep clocks running through WFI (CSLEEP).
   * Without these, WFI halts the bus/RAM clocks for the AXISRAM banks
   * this image runs from, and the core never wakes from the SysTick
   * interrupt.
   */

  putreg32(RCC_BUSLPENR_ACLKNLPEN | RCC_BUSLPENR_ACLKNCLPEN,
           STM32_RCC_BUSLPENSR);
  putreg32(RCC_MEMLPENR_ALLAXISRAM | RCC_MEMLPENR_CACHEAXIRAMLPEN,
           STM32_RCC_MEMLPENSR);
#ifdef CONFIG_STM32_USART1
  putreg32(RCC_APB2LPENR_USART1LPEN, STM32_RCC_APB2LPENSR);

  /* Route USART1's kernel clock to HSI so the BRR computation in
   * stm32n6_lowsetup() is independent of any later SYSCLK change (e.g.
   * enabling PLL1).
   */

  putreg32(RCC_CCIPR13_USART1SEL_HSI, STM32_RCC_CCIPR13);
#endif

  /* Mark the board's I/O voltage domains as supply-valid before any GPIO
   * pad is driven.  The USART1 console pins PE5/PE6 (AF7) are on GPIO
   * port E, which the datasheet (DS14791 Table 18 notes 9/10) places on
   * the VDDIO3/VDDIO2 domains; those domains reset as not-supply-valid,
   * so their pads cannot drive a level until the matching SV bits are
   * set.  Without this the CPU boots but the UART emits nothing.  The
   * PWR_SVMCR3_* mask is board-specific and supplied by board.h via
   * BOARD_PWR_VDDIO.  (PWR is on an always-on domain and needs no clock
   * gate, matching upstream's call site.)
   */

  stm32n6_pwr_enablevddio(BOARD_PWR_VDDIO);

  /* Apply the ES0620 I/O-compensation mitigation (write 0x287) to the
   * domains we use.  Only VDDIO2 and VDDIO3 are touched: the other
   * VDDIOxCCCR registers cannot be accessed without their VDDIOxSV bit
   * set first (a separate ES0620 constraint), and only VDDIO2/3 are
   * declared supply-valid in BOARD_PWR_VDDIO above.  The register
   * offsets come from ST CMSIS stm32n647xx.h (VDDIO2CCCR=0x44,
   * VDDIO3CCCR=0x4c), not from upstream nuttx (which mislabels 0x54/0x5c
   * -- those are VDDIO4/5 on this silicon).  See stm32_syscfg.h.
   */

  putreg32(SYSCFG_CCCR_ES0620_MANUAL, STM32_SYSCFG_VDDIO2CCCR);
  putreg32(SYSCFG_CCCR_ES0620_MANUAL, STM32_SYSCFG_VDDIO3CCCR);
  putreg32(SYSCFG_CCCR_ES0620_MANUAL, STM32_SYSCFG_VDDCCCR);

  /* Point the Cortex-M55 secure vector-table base at our SRAM vectors,
   * matching upstream stm32_start.c.
   */

  putreg32((uint32_t)_vectors, STM32_SYSCFG_INITSVTORCR);

  /* Read-back to ensure the prior SYSCFG writes have completed before we
   * start driving pads.
   */

  (void)getreg32(STM32_SYSCFG_VDDCCCR);

  /* Configure the UART for early debug output.  From this point on
   * showprogress() can emit characters, so the markers below let us
   * pinpoint the boot stage by the last character printed.
   */

  stm32n6_lowsetup();
  showprogress('A');

#ifdef CONFIG_STM32_RISAF_EARLY_PROBE
  /* Investigate RISAF in the earliest safe window: console up, heap not yet
   * initialised (memory layout still "virgin").  Purely observational --
   * dumps the active SRAM region map and re-confirms CIDCFGR writability,
   * enabling no region and restoring every value.  See ADR-039.
   */

  stm32n6_risaf_early_probe();
#endif

  /* Call board early initialization */

  stm32_boardinitialize();
  showprogress('B');

#ifdef USE_EARLYSERIALINIT
  arm_earlyserialinit();
#endif
  showprogress('C');

  /* Barrier after the SCB (VTOR), SYSCON and SYSCFG (INITSVTORCR) writes
   * above before handing off to NuttX.
   */

  UP_DSB();
  UP_ISB();

  /* 'D' means __start_c ran to completion; anything that faults after
   * this marker is inside nx_start() (heap init, up_irqinitialize,
   * up_timer_initialize, board late init, ...).
   */

  showprogress('D');

  /* Start NuttX */

  showprogress('\r');
  showprogress('\n');
  nx_start();

  for (; ; );
}
