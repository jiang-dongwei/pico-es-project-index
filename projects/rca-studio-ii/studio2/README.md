# RCA Studio II / RetroPicoES V2.1

状态：`studio2-perf1` 绘制优化候选，沿用 Tennis 自动启动与输入 v4。主机验证与双地址构建已完成，绘制耗时待真机复测；Tennis v3 的球运动和上下控制已获用户确认。详细范围见 `doc/HANDOFF.md`。

独立 worktree：`E:\ComporyProject\pico_ES_Public_Projects\Pico_ES_RCAStudioII`。
分支：`codex/rca-studio-ii`。端口源码位于 `studio2/`。

实现 CDP1802 指令、IRQ、DMA、CDP1861 视频时序、512 字节 RAM 镜像、ST2/BIN/ROM 卡带、双数字键盘和 Q 门控蜂鸣音。
ROM 列表、Settings 和生命周期复用公共合同 v2。ROM 页与游戏内 Menu 使用同一 Settings 宿主。

## 构建

本机只读平台依赖默认位于 `E:\ComporyProject\Pico_ES`，可用 `-WorkspaceRoot` 指定其它完整平台副本。
需要 SDK 2.3.0、Pico-PIO-USB、tusb_xinput 和已修补的 digger/pico_shared。
不复制或修改共享启动、QMI、PSRAM 和时钟代码；时钟沿共享默认 252 MHz。

```powershell
.\studio2\scripts\build-retropicoes-v21.ps1 -Bootloader
.\studio2\scripts\build-retropicoes-v21.ps1
cmake -S studio2 -B studio2/build-host -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build studio2/build-host --parallel 8
ctest --test-dir studio2/build-host --output-on-failure
.\studio2\scripts\Test-Contract.ps1
.\studio2\scripts\Test-Installer.ps1
```

MinGW 14 的 bin 必须位于 PATH 首位，避免不同 libstdc++ DLL 被宿主工具误加载。
普通版地址 `0x10000000`，Loader 版地址 `0x10080000`。详细证据见 `doc/HANDOFF.md`。

## 内容与操作

- 商业卡带放 `/roms/STUDIO2/`，BIOS 另放 `/bios/studio2.rom`，长度严格 2048 字节。发行物不含 RCA BIOS 或商业 ROM。
- 原创诊断：`python studio2/testrom/generate.py --output studio2/build-diagnostics`。两个 `.s2d` 镜像独立启动，不需要 RCA BIOS，也不替代商业 BIOS。
- `.bin/.rom` 为从 0400 开始、最大 1024 字节的原始卡带。`.st2` 使用 RCA2 页表，接受 04–07、0C–0F ROM 页；重复、截断、RAM 页覆盖均拒绝。
- Bank 0：Left=4、Right=6、Up=2、Down=8、Jump=0、Action=5、Start=1。
- Bank 1：Left=7、Right=9、Up=3、Down=0、Jump=4、Action=6、Start=5。两 Bank 覆盖数字 0–9。
- 一个控制器由 Settings 的 Keypad Player 选择 A/B；两 USB 控制器时 USB1/USB2 分别控制 A/B。
- 物理 A/B 在菜单中为确认/返回；物理 Menu/Home 单键打开 Settings，不进入数字键盘。
- Settings 包含音频、扫描线、菜单颜色、Keypad Bank/Player、Controller Test、游戏内 Quit/Reset 和返回 Loader。Reset/返回 Loader 二次确认。
- SAVE 写 `/settings_STUDIO2.dat`；CANCEL 不应用工作副本。首版不提供完整状态槽或 FPS overlay，不显示无效选项。

## 安装与验收

硬件由主窗口独占，本任务没有烧录、写 SD 或打开 COM11。主窗口确认 SD 后运行安装脚本并显式传入 `-SdRoot`。
安装器校验 Loader UF2 地址和回读哈希，维护唯一 `studio2;studio2;RCA Studio II` 索引，安装原创 320×240 封面，保留 ROM/存档。
它不自动复制游戏或诊断镜像；运行对象由硬件所有者准备。

现有共享 `perf_report.h` 固定 1 秒窗口，未提供 2 秒配置接口。这是按并行规范交回主窗口的公共层缺口。
候选复用该报告器输出 PERF/PERF IO，通过同一非阻塞队列增加 10 秒心跳；性能合同不能标记全部通过。
真机计划、未完成项与许可依据见 `doc/HANDOFF.md`、`doc/CORE_REFERENCES.md`。

## Tennis/Squash 原始数字键盘启动序列（参考）

仅按 Up 或实体 B 不等于完成启动。原版使用两个数字键盘，Tennis 的已验证启动序列为 A1 → B4 → A7。设置修改必须选择 SAVE；CANCEL/直接返回不应用修改。

1. Settings → Reset，确认；设置 Keypad Player=A、Bank=0 并 SAVE；回游戏短按 Start，再松开（A1）。
2. Settings → Player=B、Bank=0 并 SAVE；回游戏短按 Left，再松开（B4）。
3. Settings → Player=A、Bank=1 并 SAVE；回游戏短按 Left，再松开（A7）。
4. Settings → Player=B、Bank=0 并 SAVE；回游戏 Up/Down 对应 B2/B8，可操作 B 键盘的球拍。

两 USB 手柄时 USB1=A、USB2=B，Player 设置被双手柄分配覆盖；Bank 仍为两手柄共用。完整双人 Tennis 需要两手柄，单手柄也能完成启动并验证一侧球拍。

主机真实卡带探针（不自带 ROM，手工提供 512 字节 Tennis 卡带）：

```powershell
cmake --build studio2/build-host --target studio2_tennis_probe
./studio2/build-host/studio2_tennis_probe.exe /path/to/studio2.rom /path/to/TENNIS.BIN
```

探针明确执行跨 A/B 键盘的完整序列，验证双实例确定性、启动后连续画面变化和 B2 输入相对于无输入对照的画面差异。不是屏幕、按键或声音的真机 PASS。
## 游戏内重开 Tennis（v3 起保留）

进入 Tennis 后按 Menu/Back 打开公共 Settings，选择 **Start Tennis** 并确认重启，回游戏等待约 9 秒。前端自动发送 A1 → B4 → A7，包含按下与松开间隔；之后单手柄 Up/Down 控制 B 键盘球拍。无需手动切换 Player/Bank。这个动作重置当前局，设置在本次会话中改为 Player B/Bank 0，不自动写配置文件；需要永久保存时仍用 SAVE。自动设置期间物理数字键输入暂停采纳，Menu 仍可暂停；返回后继续设置序列。

该菜单项仅针对 CRC32 a21590c8、后续卡带窗口无附加内容的原版 Tennis/Squash 显示，兼容 raw BIN 和等价 ST2 页表。其他游戏仍使用原始数字键盘映射。确认取消时不重置游戏。

串口 INPUT STUDIO2 v=3 的 setup 是自动设置剩余模拟帧数，完成时为 0；自动序列期间可看到 A、B 与 ef_cycles 的变化。单手柄游戏控制为 Player B/Bank 0，双 USB 手柄仍分别控制 A、B。
## 当前版本：选择 Tennis 后直接开始（v4）

在公共 ROM 列表打开 Tennis 后自动执行原版设置，约 9 秒后进入可操作游戏，无需打开 Settings。单手柄 Up/Down 控制 B 球拍；两个 USB 手柄仍分别控制 A/B。游戏内公共 Settings 保留 Start Tennis，仅在想重开一局时使用。

Tennis 的 Player B/Bank 0 是运行期默认值，Settings 显示实际生效值；不自动改写配置文件，退出后其他卡带恢复用户原来的键盘设置。玩家主动在 Settings 修改并 SAVE 时，遵守保存的配置。自动设置可以通过 Menu 暂停和恢复，不影响其他卡带或 .s2d 原创诊断。串口版本标记为 INPUT STUDIO2 v=4。
