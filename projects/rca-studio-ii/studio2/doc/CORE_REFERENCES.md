# 来源与许可

原创 CDP1802、Studio II、诊断程序和胶水为 GPL-3.0-or-later。完整许可证见 ../LICENSE。
菜单输入、Settings 和板级主循环骨架改自 Pico_ES 基线 `7330ed6d4a0e474bd5938c2d78981550bf86e0e3` 的 chip8/platform/retropicoes，保留 GPL 标识。
common、compat 直接链接 worktree 基线；不复制公共 UI 状态机或修改接口。
原创 SVG/PNG 封面同为 GPL-3.0-or-later。诊断镜像由原创 Python 生成器产生，不含商业数据。

硬件参考，不把 MAME 框架或实现加入固件：

- [RCA CDP1802 用户手册](https://www.bitsavers.org/components/rca/cosmac/MPM-201A_User_Manual_for_the_CDP1802_COSMAC_Microprocessor_1976.pdf)：指令与寄存器语义。
- [MAME Studio II 硬件说明](https://github.com/mamedev/mame/blob/4ae561d0019312c0af7a519c2d22f52968e029e1/src/mame/rca/studio2.cpp)：内存、键盘、卡带页表。
- [MAME CDP1861 时序说明](https://github.com/mamedev/mame/blob/4ae561d0019312c0af7a519c2d22f52968e029e1/src/devices/video/cdp1861.cpp)：DMA/IRQ/EF1 时序。
- [MAME COSMAC](https://github.com/mamedev/mame/blob/4ae561d0019312c0af7a519c2d22f52968e029e1/src/devices/cpu/cosmac/cosmac.cpp)：指令交叉核对。

参考日期：2026-10-06；MAME HEAD 为以上完整提交。引用文件标注 BSD-3-Clause，版权归各自上游作者。

构建依赖：Pico SDK（BSD-3-Clause）`98a542c1a62fb549ffb5d66a3e5892b06276b670`；Pico-PIO-USB（MIT）`fe9133fc513b82cc3dc62c67cb51f2339cf29ef7`；tusb_xinput（MIT）`915b21ac3d86d279d4e2b52384f1910ac633a607`。
patched pico_shared、FatFS、TinyUSB、lwmem 和 Frens 平台保留已有逐组件许可证，不能全部改称 GPL。
共享构建文件的 SHA-256 在 dependencies-lock.json。使用主工作区只读快照，不宣称干净上游可自动复现。

未纳入商业 BIOS/卡带，也未验证商业游戏兼容性。CDP1802 与机器层分离，可供后续 COSMAC ELF 复用；本次没有实现 ELF 面板或其加载模式。
