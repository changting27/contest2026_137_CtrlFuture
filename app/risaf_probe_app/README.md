# risaf_probe_app（RISAF 防火墙可写性探针）

映射到 openvela `packages/demos/contest2026_137_risaf_probe_app`。

ADR-039 决定性实验：判定 **DEV boot 下 NuttX 固件能否自己写 RISAF 配置
寄存器**。这是解锁 ADR-029/032（DMA→SRAM 被 RISAF 防火墙拦截）的关键分叉点。

`risaf_probe` 调用 arch 助手 `stm32n6_risaf_probe()`。之前的 DMA2D 探针只
改了**主设备侧**（RIMC 里 DMA2D 的 CID），从没碰过**内存区域侧**（RISAF
region 的 CIDCFGR 白名单）。本探针单独测区域侧，且**不改变任何访问强制**：

- 目标 RISAF21 REG[0]（本板实测为禁用态 BREN=0、容器未锁 GLOCK=0）；
- 全程保持 BREN=0：禁用的 region 不被防火墙评估，写它的 CIDCFGR 不改变
  CPU 或任何 DMA 主的可访问范围，**无锁死风险**；
- 往 CIDCFGR 写一个完全定义的位图样（CID 0..7 读+写使能 = 0x00FF00FF），
  读回后立即恢复原值。

读回 == 图样 ⇒ **固件可写 RISAF** ⇒ 只要把 DMA 主的 CID 加进 SRAM region
写白名单即可解锁 ADR-029/032，**无需 flash-boot**。读回为 0/不变（RAZ/WI）
⇒ RISAF 配置空间被锁到更高特权 ⇒ 只能走 ADR-039 的自定义 FSBL 路径。

安全性：决不置 GLOCK、决不使能 region、决不动 RISAF7（固件自身运行的
FLEXMEM）。全程片内，无需外部接线或仪器。

依赖 `CONFIG_STM32_RISAF_PROBE`。
