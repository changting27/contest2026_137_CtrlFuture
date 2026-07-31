# dma2d_unlock_app（DMA2D RISAF 解锁实验样例）

映射到 openvela `packages/demos/contest2026_137_dma2d_unlock_app`。

ADR-039 DEV-boot 子课题实验：普通 DMA2D 探针（`dma2d_test`）已测得
DEV boot 下整芯片**没有任何 enabled RISAF region**，DMA2D 写 SRAM 是被
RISAF **默认策略**丢弃的，而非被某个 region 白名单排除。本实验直接验证修法。

`dma2d_unlock` 调用 arch 助手 `stm32n6_dma2d_unlock_probe()`——它在
**RISAF2**（管辖 AXI SRAM `0x34000000`，固件与缓冲即运行于此）的 REG[0]
上建一个覆盖缓冲的 enabled region，写白名单 = CID0（DMA2D）+ CID1（CPU/
TDCID），再用 RIMC 扫 DMA2D master CID 0..7 各跑一次 register-to-memory
填充，观察哪些落地。

STARTR/ENDR 是相对 `0x34000000` 的字节偏移（非绝对地址），4 KiB 粒度、
上下界皆 inclusive。预期：**CID0/CID1 落地、CID2..7 被拦**——证明 region
白名单是按配置强制执行的，NuttX 自建 RISAF region 即可在 DEV boot 下解锁
DMA→SRAM，无需自定义 FSBL。

安全性：region 先写好 bounds+白名单再最后置 BREN；白名单恒含 CID1，CPU
永不被锁在自己的 SRAM 外；每个改动寄存器结束时按位还原到 boot 状态；DEV
boot 下任何误配置复位即恢复（RISAF 配置易失）。全程片内，无需外部接线或仪器。

依赖 `CONFIG_STM32_DMA2D_UNLOCK`。
