/****************************************************************************
 * arch/arm/src/stm32n6/hardware/stm32_memorymap.h
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

#ifndef __ARCH_ARM_SRC_STM32N6_HARDWARE_STM32_MEMORYMAP_H
#define __ARCH_ARM_SRC_STM32N6_HARDWARE_STM32_MEMORYMAP_H

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* STM32N6 Memory Map (non-secure aliases)
 *
 * Each peripheral is physically aliased twice: a Non-Secure alias at
 * 0x4xxxxxxx and a Secure alias at 0x5xxxxxxx (RM0486 section 3.5.1).
 * The same hardware register is reached through either alias.  This
 * port intentionally keeps the Non-Secure aliases so that, if this
 * board is ever split into a Secure/Non-Secure TrustZone image pair,
 * NuttX (the larger, general-purpose OS) can keep running unmodified
 * as the Non-Secure world's OS -- the small Secure-world firmware
 * that would configure SAU/RIFSC/NVIC target-state and perform the
 * actual Secure->Non-Secure handoff is a separate, not-yet-written
 * component.  In the current single-world (Secure-only, no SAU
 * configured) boot configuration, both aliases reach the same
 * register and are functionally equivalent.
 */

#define STM32_SRAM_BASE        0x34000000ul
#define STM32_PERIPH_BASE      0x40000000ul

/* Bus base addresses */

#define STM32_APB1_BASE        (STM32_PERIPH_BASE + 0x00000000ul)
#define STM32_AHB1_BASE        (STM32_PERIPH_BASE + 0x00020000ul)
#define STM32_APB2_BASE        (STM32_PERIPH_BASE + 0x02000000ul)
#define STM32_AHB2_BASE        (STM32_PERIPH_BASE + 0x02020000ul)
#define STM32_APB3_BASE        (STM32_PERIPH_BASE + 0x04000000ul)
#define STM32_AHB3_BASE        (STM32_PERIPH_BASE + 0x04020000ul)
#define STM32_APB4_BASE        (STM32_PERIPH_BASE + 0x06000000ul)
#define STM32_AHB4_BASE        (STM32_PERIPH_BASE + 0x06020000ul)
#define STM32_APB5_BASE        (STM32_PERIPH_BASE + 0x08000000ul)
#define STM32_AHB5_BASE        (STM32_PERIPH_BASE + 0x08020000ul)

/* APB1 peripherals */

#define STM32_TIM2_BASE        (STM32_APB1_BASE + 0x0000)
#define STM32_TIM3_BASE        (STM32_APB1_BASE + 0x0400)
#define STM32_TIM4_BASE        (STM32_APB1_BASE + 0x0800)
#define STM32_TIM5_BASE        (STM32_APB1_BASE + 0x0c00)
#define STM32_LPTIM1_BASE      (STM32_APB1_BASE + 0x2400)
#define STM32_WWDG_BASE        (STM32_APB1_BASE + 0x2c00)
#define STM32_USART2_BASE      (STM32_APB1_BASE + 0x4400)
#define STM32_USART3_BASE      (STM32_APB1_BASE + 0x4800)
#define STM32_UART4_BASE       (STM32_APB1_BASE + 0x4c00)
#define STM32_UART5_BASE       (STM32_APB1_BASE + 0x5000)
#define STM32_I2C1_BASE        (STM32_APB1_BASE + 0x5400)
#define STM32_I2C2_BASE        (STM32_APB1_BASE + 0x5800)
#define STM32_I2C3_BASE        (STM32_APB1_BASE + 0x5c00)
#define STM32_UART7_BASE       (STM32_APB1_BASE + 0x7800)
#define STM32_UART8_BASE       (STM32_APB1_BASE + 0x7c00)

/* APB2 peripherals */

#define STM32_TIM1_BASE        (STM32_APB2_BASE + 0x0000)
#define STM32_USART1_BASE      (STM32_APB2_BASE + 0x1000)
#define STM32_TIM15_BASE       (STM32_APB2_BASE + 0x4000)
#define STM32_TIM16_BASE       (STM32_APB2_BASE + 0x4400)
#define STM32_TIM17_BASE       (STM32_APB2_BASE + 0x4800)
#define STM32_USART6_BASE      (STM32_APB2_BASE + 0x1400)
#define STM32_UART9_BASE       (STM32_APB2_BASE + 0x1800)
#define STM32_USART10_BASE     (STM32_APB2_BASE + 0x1c00)

/* APB4 peripherals */

#define STM32_LPUART1_BASE     (STM32_APB4_BASE + 0x0c00)
#define STM32_LPTIM2_BASE      (STM32_APB4_BASE + 0x2400)
#define STM32_LPTIM3_BASE      (STM32_APB4_BASE + 0x2800)
#define STM32_LPTIM4_BASE      (STM32_APB4_BASE + 0x2c00)
#define STM32_LPTIM5_BASE      (STM32_APB4_BASE + 0x3000)
#define STM32_RTC_BASE         (STM32_APB4_BASE + 0x4000)
#define STM32_TAMP_BASE        (STM32_APB4_BASE + 0x4400)
#define STM32_IWDG_BASE        (STM32_APB4_BASE + 0x4800)
#define STM32_BSEC_BASE        (STM32_APB4_BASE + 0x9000)
#define STM32_DTS_BASE         (STM32_APB4_BASE + 0xa000)

/* SYSCFG lives on APB4 at +0x8000.  CMSIS stm32n647xx.h places SYSCFG_NS
 * at APB4PERIPH_BASE_NS + 0x8000 = 0x46008000; this port uses the
 * non-secure alias throughout (APB4 base 0x46000000).
 */

#define STM32_SYSCFG_BASE      (STM32_APB4_BASE + 0x8000)

/* AHB4 peripherals
 *
 * CMSIS stm32n647xx.h confirms this chip has GPIO ports A-H plus
 * N/O/P/Q (12 ports total) -- there is no GPIOI/GPIOJ/GPIOZ_BASE_NS
 * defined anywhere in CMSIS for this part.  A previous revision of
 * this header defined a fictitious GPIOI/GPIOJ/GPIOZ port set; that
 * was never referenced by any board-level code and has been removed.
 */

#define STM32_GPIOA_BASE       (STM32_AHB4_BASE + 0x0000)
#define STM32_GPIOB_BASE       (STM32_AHB4_BASE + 0x0400)
#define STM32_GPIOC_BASE       (STM32_AHB4_BASE + 0x0800)
#define STM32_GPIOD_BASE       (STM32_AHB4_BASE + 0x0c00)
#define STM32_GPIOE_BASE       (STM32_AHB4_BASE + 0x1000)
#define STM32_GPIOF_BASE       (STM32_AHB4_BASE + 0x1400)
#define STM32_GPIOG_BASE       (STM32_AHB4_BASE + 0x1800)
#define STM32_GPIOH_BASE       (STM32_AHB4_BASE + 0x1c00)
#define STM32_GPION_BASE       (STM32_AHB4_BASE + 0x3400)
#define STM32_GPIOO_BASE       (STM32_AHB4_BASE + 0x3800)
#define STM32_GPIOP_BASE       (STM32_AHB4_BASE + 0x3c00)
#define STM32_GPIOQ_BASE       (STM32_AHB4_BASE + 0x4000)
#define STM32_EXTI_BASE        (STM32_AHB4_BASE + 0x5000)
#define STM32_RCC_BASE         (STM32_AHB4_BASE + 0x8000)

/* AHB1 peripherals */

#define STM32_GPDMA1_BASE      (STM32_AHB1_BASE + 0x1000)
#define STM32_ADC1_BASE        (STM32_AHB1_BASE + 0x2000)
#define STM32_ADC2_BASE        (STM32_AHB1_BASE + 0x2100)
#define STM32_ADC12_COMMON_BASE (STM32_AHB1_BASE + 0x2300)

/* AHB3 peripherals */

#define STM32_RNG_BASE         (STM32_AHB3_BASE + 0x0000)
#define STM32_HASH_BASE        (STM32_AHB3_BASE + 0x0400)
#define STM32_RIFSC_BASE       (STM32_AHB3_BASE + 0x4000)
#define STM32_RISAF7_BASE      (STM32_AHB3_BASE + 0xc000)

/* AHB5 peripherals */

#define STM32_DMA2D_BASE       (STM32_AHB5_BASE + 0x1000)
#define STM32_GPU2D_BASE       (STM32_AHB5_BASE + 0x14000)

/* APB5 peripherals (display and camera).  RM0486's memory map lists the
 * Secure aliases at 0x5800_0000 (LTDC) and 0x5800_1000 (DCMIPP); the
 * Non-Secure aliases used throughout this port are APB5_BASE-relative
 * (0x4800_0000 / 0x4800_1000).  Their clocks live in RCC_APB5ENR.
 */

#define STM32_LTDC_BASE        (STM32_APB5_BASE + 0x0000)
#define STM32_DCMIPP_BASE      (STM32_APB5_BASE + 0x1000)

#endif /* __ARCH_ARM_SRC_STM32N6_HARDWARE_STM32_MEMORYMAP_H */
