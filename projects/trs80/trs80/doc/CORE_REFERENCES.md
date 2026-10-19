# 核心来源与许可证

- 来源：[TimothyPMann/xtrs](https://github.com/TimothyPMann/xtrs)。
- 固定提交：`3a2180c063811a715faa28d39a94739c33e0abd0`。
- 导入组件：`z80.c`、`z80.h`、`config.h`、`trs.h`、`trs_imp_exp.h`、`trs_chars.c`、`LICENSE`。
- Clarendon Hill Software 1992 原始许可要求保留版权与免责说明；Timothy P. Mann
  增量为 MIT 式许可。所有原始文件头与仓库 LICENSE 已保留。
- 项目新增适配器、装载器、测试、虚拟键盘采用 GPL-3.0-or-later。
- `third_party/xtrs/source-lock.json` 记录上游与当前文件 SHA-256。
- `patches/z80.c.patch` 去掉未使用的 rand/time 引入与 CPU reset 的墙钟随机种子。
- `patches/z80.h.patch` 去掉 Unix `sys/time.h`，将 T-state 固定为 uint64_t，避免
  Windows LLP64 与 Cortex-M33 上的计数宽度不一致。

内存映射、键盘矩阵、端口和定时参数对照同一上游提交的 `trs_memory.c`、
`trs_keyboard.c`、`trs_io.c`、`trs_interrupt.c`；这些文件不直接编入此移植。
没有导入桌面 X11、GTK、POSIX 主机文件陷阱或商业游戏源码。
`trs_chars.c` 为固定上游随附的字形数据，保留其原始来源说明；补丁只去掉桌面头文件并改为只读数组。

CPU 保留 xtrs 的单实例结构，由 core0 独占；适配器不在 core1 调用 Z80。
IM0/IM2 与部分未定义指令继承上游限制；不把少量测试称为全部 Z80 正确性证明。
宿主陷阱显式返回 CPU_FAULT，不执行 shell 或任意主机文件操作。

板级基础 RAM 64 KiB、BIOS 14 KiB、视频 2 KiB 保留 SRAM；64 KiB 扩展 RAM 与
媒体暂存显式在 PSRAM。完整链接 SRAM 用量见 HANDOFF。公共 QMI/PSRAM 初始化保持基线。
