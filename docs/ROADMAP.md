# ROADMAP — STM32N647X0 板级适配

> 详细功能清单见 [FEATURE-LIST.md](FEATURE-LIST.md)。

## Phase 定义

| Phase | 名称 | 目标 | ADR 范围 |
|-------|------|------|----------|
| **P0** | 最小 NSH 启动 | 真机串口输出 `nsh>` | 001-004 |
| **P1** | 基础外设 | 完整时钟树、GPIO、DMA、SPI/I2C | 005-017 |
| **P2** | 存储 | XSPI Flash 启动、SDMMC | 018-021 |
| **P3** | 通信 | Ethernet、USB、FDCAN | 022-024 |
| **P4** | 高级功能 | NPU、Camera、Display、Security | 025-038 |

> P0/P1 为核心目标，P2/P3 为延伸目标，P4 为远期规划。

> **验证口径说明 (2026-07-30)**：真机（远程 STM32N647 两跳 SSH + GDB
> 加载 SRAM + 串口采集）现已可用，ADR-004 启动链真机 MEASURED，
> 其后每个影响真机行为的 ADR（027/028/029/030/032/037…）均经
> `hw-verify-drivertest.sh` 真机回归。「验证」列为目标级别；「状态」列
> 反映已达级别。**PARTIAL** = 设备框架已注册但硬件路径受阻（RISAF/
> boot-mode 授权，非固件可修复）或寄存器路径未实现；**N/A** = ADR
> 决策不实现。

---

## P0: 最小 NSH 启动

> 目标: STM32N6 开发板通过 DEV boot 从 SRAM 启动 NuttX，USART1 输出 NSH 命令行。

| # | 模块 | ADR | 依赖 | 验证 | 状态 |
|---|------|-----|------|------|------|
| 1 | CI 集成（symlink + Kconfig patch） | [001](adr/ADR-001.md) | — | BUILD | **DONE** |
| 2 | 最小启动（RCC/GPIO/USART1/IRQ/heap） | [002](adr/ADR-002.md) | — | BUILD | **DONE** |
| 3 | SysTick 系统节拍 | [003](adr/ADR-003.md) | 002 | BUILD | **DONE** |
| 4 | 真机串口验证 | [004](adr/ADR-004.md) | 002, 003 | MEASURED | **DONE** |

## P1: 基础外设

> 目标: 完整时钟树、全引脚 GPIO、DMA、SPI/I2C 总线、看门狗。

| # | 模块 | ADR | 依赖 | 验证 | 状态 |
|---|------|-----|------|------|------|
| 5 | RCC 时钟树完善 | [005](adr/ADR-005.md) | 004 | MEASURED | **DONE** |
| 6 | GPIO 完善（165 引脚 AF） | [006](adr/ADR-006.md) | 005 | BUILD | **DONE** |
| 7 | Cache 配置（ICACHE + DCACHE） | [007](adr/ADR-007.md) | 005 | MEASURED | **DONE** |
| 8 | TCM 配置（DTCM + ITCM） | [008](adr/ADR-008.md) | 005 | MEASURED | **DONE** |
| 9 | PWR 电源管理 | [009](adr/ADR-009.md) | 005 | MEASURED | **DONE** |
| 10 | EXTI 扩展中断 | [010](adr/ADR-010.md) | 006 | QEMU | **DONE** |
| 11 | GPDMA1 通用 DMA | [011](adr/ADR-011.md) | 005, 007 | MEASURED | **DONE** |
| 12 | 额外 USART/UART | [012](adr/ADR-012.md) | 005, 006 | MEASURED | **PARTIAL**（USART1 控制台真机；额外端口仅 Renode） |
| 13 | SPI 驱动（SPI1-6） | [013](adr/ADR-013.md) | 006, 011 | MEASURED | **PARTIAL**（驱动完整；未接线、无真机总线验证） |
| 14 | I2C 驱动（I2C1-4） | [014](adr/ADR-014.md) | 006, 011 | MEASURED | **DONE**（I2C4 真机；板载 AP3216C@0x1E 写/回读握手） |
| 15 | 看门狗（IWDG/WWDG） | [015](adr/ADR-015.md) | 005 | MEASURED | **DONE** |
| 16 | RTC 实时时钟 | [016](adr/ADR-016.md) | 005 | MEASURED | **DONE** |
| 17 | MPU 内存保护 | [017](adr/ADR-017.md) | 007, 008 | QEMU | **DONE** |

> - **012 PARTIAL**：USART1 控制台真机 MEASURED（见 ADR-004）；驱动保留
>   USART2/3 多端口框架（`#ifdef` 守护），但仅 USART1 编译使能，
>   额外端口只有 Renode 寄存器访问测试，无真机多串口数据往返。
> - **013 PARTIAL**：`stm32n6_spi.c` 是完整的寄存器编程驱动（SPI1-6
>   base/CFG1/CPOL/CPHA + exchange），但未接入任何 defconfig、无板级引脚
>   接线、无真机总线验证（SPI 无片内自环，需外部 MOSI-MISO 跳线）。
>   原 ADR 文档「暂不实现」已 stale——驱动实际存在。
> - **014 DONE**（2026-07-31 真机 MEASURED）：I2C4 修正为 PE13/PE14 AF4、
>   补齐 APB4 时钟使能 + HSI 内核时钟、修复 repeated-start TC 竞态，接入
>   nsh-test defconfig。板载真实从机是 AP3216C(@0x1E)（**非** SHT30——后者
>   为 EdgeSight 规划注释，非本板硬件）。`ap3216c_test` 免跳线自测软复位后
>   写 0x03 到系统配置寄存器再回读，断言 == 0x03，真机 `AP3216C PASS`。

## P2: 存储

> 目标: 从外部 Flash 启动，SD 卡文件系统。

| # | 模块 | ADR | 依赖 | 验证 | 状态 |
|---|------|-----|------|------|------|
| 18 | XSPI 接口驱动 | [018](adr/ADR-018.md) | 005, 006, 011 | MEASURED | **PARTIAL**（仅 Renode 模型；无真机 XSPI 往返，需 flash-boot） |
| 19 | XSPI Flash 启动 | [019](adr/ADR-019.md) | 018 | MEASURED | **PARTIAL**（仅 Renode 回归；真机全程 DEV boot，未 flash-boot 冷启动） |
| 20 | SDMMC 驱动 | [020](adr/ADR-020.md) | 005, 006, 011 | MEASURED | **DONE** |
| 21 | FMC 存储控制器 | [021](adr/ADR-021.md) | 005, 006 | MEASURED | |
| 39 | Flash 冷启动（FSBL + XSPI） | [039](adr/ADR-039.md) | 018, 019 | MEASURED | **PROPOSED**（路线固化，暂不动手；掉电持久冷启动，不含 RISAF 解锁） |

## P3: 通信

> 目标: 网络连接、USB 设备。

| # | 模块 | ADR | 依赖 | 验证 | 状态 |
|---|------|-----|------|------|------|
| 22 | Ethernet GMAC (1G) | [022](adr/ADR-022.md) | 005, 006, 011, 007 | MEASURED | **DONE** |
| 23 | USB OTG HS | [023](adr/ADR-023.md) | 005, 006, 009 | MEASURED | **PARTIAL**（骨架；host 模式未实现，无真机验证） |
| 24 | FDCAN | [024](adr/ADR-024.md) | 005, 006 | MEASURED | **DONE** |

> - **023 PARTIAL**：`stm32n6_otg.c` 仅寄存器骨架（host 模式
>   `return -ENOSYS`，多处 TODO），未接入 defconfig。USB 需物理主机
>   枚举，远程真机无法自环验证；EdgeSight 数据路径不走 USB，延后至 P5。

## P4: 高级功能

> 目标: NPU 推理、Camera 图像采集、显示输出、安全启动。

| # | 模块 | ADR | 依赖 | 验证 | 状态 |
|---|------|-----|------|------|------|
| 25 | HPDMA1 高性能 DMA | [025](adr/ADR-025.md) | 011 | — | **N/A**（决策不实现） |
| 26 | 高级定时器（TIM1/TIM8） | [026](adr/ADR-026.md) | 005 | — | **N/A**（决策不实现） |
| 27 | 通用定时器 | [027](adr/ADR-027.md) | 005 | MEASURED | **MEASURED** |
| 28 | 低功耗定时器（LPTIM1-5） | [028](adr/ADR-028.md) | 005, 009 | MEASURED | **MEASURED** |
| 29 | ADC 驱动 | [029](adr/ADR-029.md) | 005, 006, 011 | MEASURED | **PARTIAL**（轮询 MEASURED，DMA RISAF 阻塞延后） |
| 30 | DTS 温度传感器 | [030](adr/ADR-030.md) | 005 | MEASURED | **MEASURED**（dts_temp 真机读回 36.19°C） |
| 31 | LTDC LCD 控制器 | [031](adr/ADR-031.md) | 005, 006, 007, 025 | MEASURED | **PARTIAL** |
| 32 | GPU2D / DMA2D | [032](adr/ADR-032.md) | 031 | MEASURED | **PARTIAL**（探针真机 MEASURED，DMA2D→SRAM 被 RISAF 阻塞延后） |
| 33 | DCMIPP 摄像头管线 | [033](adr/ADR-033.md) | 005, 006, 025 | MEASURED | **PARTIAL** |
| 34 | CSI-2 MIPI 接口 | [034](adr/ADR-034.md) | 033 | MEASURED | |
| 35 | H.264 编码器 | [035](adr/ADR-035.md) | 033 | MEASURED | |
| 36 | Neural-ART NPU | [036](adr/ADR-036.md) | 005, 007, 008, 025 | MEASURED | |
| 37 | 加密加速器（RNG, HASH; SAES N/A） | [037](adr/ADR-037.md) | 005 | MEASURED | RNG+HASH 真机；SAES 硅片缺失 |
| 38 | Secure Boot | [038](adr/ADR-038.md) | 037, 019 | MEASURED | |

> - **025 N/A**：ADR-025 决策不实现独立 HPDMA 驱动，传输由
>   DCMIPP/VENC/NPU 硬件管道及 ST 中间件自管理。
> - **026 N/A**：ADR-026 决策不实现 TIM1/TIM8 高级定时器驱动
>   （EdgeSight 无电机控制/互补 PWM/死区需求）；通用定时/PWM/输入
>   捕获需求由 ADR-027 覆盖并真机 MEASURED。
> - **029 PARTIAL**：ADR-029 ADC VREFINT 轮询真机 MEASURED、ADC2
>   scan/AWD 已实现；ADC DMA 路径在 DEV boot 下写 SRAM 被 RISAF 拦截。
>   **更正（2026-07-31，决定性）**：DEV boot 下 NuttX 无法解锁 ADC-DMA→SRAM。
>   `dma2d_unlock` 带寄存器回读真机实测证明：即使建 region 授权 DMA CID
>   （region+RIMC 均确认生效），DMA 写 SRAM 仍被拦，门在主设备总线身份而非
>   region 白名单。ADC-DMA 在 DEV boot 保持 PARTIAL（轮询模式已 MEASURED）；
>   FSBL/secure-world 能否解属未测。详见 ADR-039「决定性更正」节。
> - **032 PARTIAL**：ADR-032 DMA2D 去风险探针真机 MEASURED——逐 CID 0..7
>   的 register-to-memory 写入均被 RISAF 防火墙拦截（含 CPU 的 CID 1）。
>   **更正（2026-07-31，决定性）**：曾推测「建专属 region 授权 DMA CID 即可
>   在 NuttX 早期解锁」，`dma2d_unlock` 真机证伪——region 使能/边界/白名单 +
>   RIMC CID override 全部回读=写入值，DMA2D 写 SRAM 仍对所有 CID（含 CID1）
>   被丢。判据：CPU@CID1 通过、DMA2D@CID1 被拦 → 门在主设备身份，非 region
>   白名单。**DEV boot 下 NuttX 无解，032 保持 PARTIAL**；FSBL 能否解属未测。
>   详见 ADR-039「决定性更正」节。
> - **031/033 PARTIAL**：`/dev/fb0`、`/dev/video0` 设备框架已注册，
>   但 LTDC 寄存器编程与 DCMIPP CMW_CAMERA 调用均未实现；
>   两者未在任何 defconfig 启用（`CONFIG_VIDEO_FB`/`CONFIG_VIDEO`
>   门控，CI 不编译）。详见 ADR 内「实现现状」。
> - **037 MEASURED（SAES N/A）**：RNG（`/dev/random` 熵健全性）与 HASH
>   （SHA-256("abc") 对 NIST 向量）均真机 MEASURED。SAES/CRYP 判定 N/A —
>   STM32N647X0 硅片不含密码学加速器（`SAES_TypeDef`/`CRYP_TypeDef` 仅存在于
>   N655/N657 SKU 的 CMSIS 头），非文档缺失。
> - **039 PROPOSED**：flash-boot + FSBL + XSPI **掉电持久冷启动**路线专题。
>   真机最终产品需脱离 DEV boot 的 SRAM+GDB 加载、独立上电启动。分阶段
>   （阶段 1 最小冷启动 → 阶段 2 可选确认 flash-boot 下 RISAF region 未被
>   GLOCK 冻结）。可逆、不烧熔丝、有 DFU 砖机安全网。**当前仅固化路线，待
>   DEV boot 下可做的 ADR 清完后启动。**
>   **范围更正（2026-07-31，决定性）**：RISAF 解锁 DMA/NPU→SRAM 与本 ADR
>   无关——但更重要的是，`dma2d_unlock` 带回读真机实测已排除**任何 NuttX 侧
>   （region+RIMC）解锁方案**：门在主设备总线身份，非固件可配。029/032/036
>   在 DEV boot 保持 PARTIAL，无 NuttX 解法；FSBL/secure-world 能否解属未测，
>   不作承诺。详见 ADR-039「决定性更正」节。

---

## 依赖关系图

```
P0                          P1                          P2          P3
───                         ───                         ───         ───
001 CI ──────────┐
                 │
002 最小启动 ────┤
  │              │
  └─ 003 SysTick ┤
       │         │
       └─ 004 真机验证
              │
              └─ 005 RCC 完善 ─────────────────────────────────────────┐
                   │                                                   │
          ┌────────┼──────────┬──────────┬──────────┐                  │
          │        │          │          │          │                  │
        006 GPIO  007 Cache  008 TCM   009 PWR   015 WDG             │
          │        │          │          │        016 RTC             │
          │        │          │          │                            │
        010 EXTI  │          │          │                            │
          │       011 DMA ←──┘          │                            │
          │        │                    │                            │
     ┌────┼────┬───┤                    │                            │
     │    │    │   │                    │                            │
   012   013  014  │               017 MPU ← 007,008                │
   UART  SPI  I2C  │                                                │
                   │                                                │
              ┌────┼────┐                                           │
              │    │    │                                           │
            018   020   021                                        │
            XSPI  SDMMC FMC                                        │
              │                                          ┌──────────┤
            019                                        022 ETH    023 USB
            XSPI Boot                                  024 FDCAN
              │
              ├──────── 039 Flash 冷启动 (FSBL + XSPI) ← 018,019
              │           (掉电持久独立上电启动；与 RISAF 解锁无关)
              │
              │   029 ADC-DMA / 032 DMA2D / 036 NPU 写 SRAM: DEV boot 下
              │     NuttX 无解 (dma2d_unlock 回读实测: region+RIMC 均生效,
              │     DMA@CID1 仍被拦 → 门在主设备总线身份, 非 region 白名单)
              │     → 三者保持 PARTIAL; FSBL/secure-world 能否解属未测
              │
              └───────── 038 Secure Boot ← 037 Crypto

P4 (高级功能)
───
025 HPDMA ← 011 DMA　【N/A：决策不实现独立驱动，031/033/036 的
  │                    传输由硬件管道 / ST 中间件自管理】
  │
  ├──→ 031 LTDC ← 005,006,007 ──→ 032 GPU2D/DMA2D
  │      │
  ├──→ 033 DCMIPP ← 005,006 ──→ 034 CSI-2 ──→ 035 H.264
  │
  └──→ 036 NPU ← 005,007,008

026 高级定时器 ← 005
027 通用定时器 ← 005
028 低功耗定时器 ← 005,009
029 ADC ← 005,006,011
030 DTS 温度传感器 ← 005
037 加密加速器 ← 005 ──→ 038 Secure Boot ← 019
```

## Renode（驱动正确性仿真）

Phase-1 **driver-correctness** 波次（Wave 0–3）已完成：Wave 相关 C#
模型 + Robot suite 覆盖上述 P0–P4 中已落地驱动的 L1/L2/L3 行为。

- 权威保真度矩阵与 **Robot suite 编号 ≠ ADR 编号** 对照表：
  [`tests/renode/README.md`](../tests/renode/README.md)
- 方法论摘要：[`DEV-METHODOLOGY.md`](DEV-METHODOLOGY.md) 中
  “Renode Fidelity Matrix (Phase-1)” 一节
- 本表各 ADR 的 **DONE / PENDING 状态不因仿真波次改写**；
  Renode 保真度以 README 矩阵为准，不在此重复逐行打标

---

## 风险登记

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| 无真机硬件 | P0.4 无法验证 | 先用 QEMU 验证至 BUILD+QEMU 级别 |
| 上游 NuttX 未合入 stm32n6 | nsh 编译依赖 Kconfig patch | CI 用 symlink + patch 方案绕过 |
| PLL 时钟配置错误 | 系统无法启动 | 先用 HSI 64MHz 直连，验证后再切 PLL |
| DMA 与 Cache 一致性 | 数据损坏 | ADR-011 依赖 ADR-007，强制先做 Cache |
