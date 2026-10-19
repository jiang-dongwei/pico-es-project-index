# TRS80 并行任务交接：trs80-readonly1

- 分支 `codex/trs80-model-i-iii-4`；基线 `7330ed6`；前一核心基线提交 `c44b54d`。
- Worktree：`E:/ComporyProject/pico_ES_Public_Projects/Pico_ES_TRS80`。
- 修改仅在 `trs80/**`、本任务 discussion 和本任务 HVR；没有改公共模块、其他端口或中央状态。
- 主机：15/15 CTest PASS；涵盖 CPU/映射/键盘/装载/中断/确定性、FDC、RGB565/音频；
  外部数据测试覆盖 I/III BASIC、4 兼容 BASIC、I/III LDOS 和 4 原生 LS-DOS 日期提示。
- 视频主机产物：`trs80/build-host/model4.png`、`frogger.png`；不是 HDMI 真机截图。
- 公共前端采用门槛 PASS；它没有自动纳入 TRS80，因此另检查 TRS80 CMake 链接三个模块、
  产品循环实际调用三个 API；C++ 主程序 Cortex-M33 严格编译 PASS，UI 真机尚未验收。
- 双地址 HW16 UF2 构建 PASS，252 MHz，SRAM 455540/524288 B (86.89%)，Scratch X/Y 各 2 KiB。
  动态 PSRAM：扩展 RAM 64 KiB、媒体最多 1 MiB；基础 64 KiB RAM 留 SRAM。

| 镜像 | 地址 | 大小 | SHA-256 |
|---|---|---|---|
| loader | `0x10080000` | 533504 B | `05DF72FD3C98B44EB71023A0F9F5FB8DDC772D9869FDBC9EAED0F33936DE059F` |
| standalone | `0x10000000` | 533504 B | `49CD74DF1A55380DB1D5FC17DCA842103E5783D62A2BB672524A8A66A35FCAA0` |

路径：`trs80/build-pico-hw16-loader/trs80.uf2` 和
`trs80/build-pico-hw16-standalone/trs80.uf2`。普通启动固件不能装入 Loader 应用目录。
源码构建命令脚本见 README；本地工具配置 `.em_skill.json` 忽略提交。
Pico SDK/Pico-PIO-USB/tusb_xinput 使用主项目现有 `_deps`，未编辑依赖源码。
共享平台来自本工作树固定 digger 基线，未从主目录拷入其他任务未提交的改动。
第三方已知 PIO-USB inline/noinline 警告仍在，任务自己的 C/C++ 严格检查通过。

## 本地交付和安装验证

`trs80/build-sd-test-package` 已包含 Loader UF2、原创 PNG、索引、三份 BIOS、
三个系统盘、Frogger/Castle/Gobbler、来源哈希和原始通知。它是工作树内临时目录，
没有连接或写入真实 SD。媒体按已确认格式改名为 JV1/JV3，不根据扩展猜 DSK 格式。

安装器临时目录验证：保留其他核心、去重 TRS80、重复运行索引哈希一致、
UF2/封面独立回读一致、普通启动 UF2 拒绝。已有 32 项且需替换时必须显式指定条目。
源索引首次备份后保留。内测数据授权边界见 RUNTIME_MEDIA；未将 BIOS/游戏提交 Git。

## 当前限制和待验收

只读 JV1/单头块 JV3；读地址/整轨、双头块 JV3、DMK、CAS、盘写入、Orchestra、
完整状态序列化、motor-off NMI 尚未实现。普通 CMD 冷启动通过通用 BIOS L/ENTER
初始化再装载，不按游戏指纹或名称绕过。FDC 仅对当前支持命令建立主机证据。

初次候选交接时未部署硬件；现已按用户后续“继续吧，H盘已接入”指令完成 H 盘文件安装，
未访问 COM11/BOOTSEL，未直接烧录设备；没有真机性能结果，不标记任务整体 PASS。
主窗口按 PARALLEL_DEVELOPMENT 5.5 检查并合并后统一部署；集中真机清单见
`.emv2/checkpoints/HVR-TRS80-readonly1.md`。需确认 ROM 有/无两种浏览器、两入口 Settings、
三机型系统启动、两个差异明显游戏、虚拟键盘发送释放、声音/暂停恢复、退出及长时性能。

## 2026-10-09 H 盘部署

用户明确指示继续并说明 H 盘已接入；本次授权覆盖 TRS80 SD 内容安装。
确认卷 H: / exFAT / 可移动 / 序列号 7AD1442C；连续读取索引 SHA-256 一致。
安装 Loader UF2、封面和唯一 TRS80 索引行，复制三份 BIOS、七份系统盘/游戏及八份原始通知。
18 个媒体/通知文件和固件均回读 SHA-256 一致，其他 67 个 emu/16 文件哈希不变，
其他索引行不变；SAVES 未写入。UF2 SHA-256 为上表 loader 值。

卡上路径：H:/emu/16/trs80.uf2、H:/data/trs80/bios/、H:/roms/TRS80/。
原始索引备份：本地 trs80/build-deploy-H/20261009-160243/emulators-before.txt，
卡上 H:/emu/emulators.txt.trs80.bak。完整报告位于同一本地目录 deployment.json。
SD 部署 PASS；Loader 实际加载、HDMI、音频、手柄及性能仍 PENDING。
先用默认 Model I 打开 Model_I/FROGGER.CMD；Model III/4 系统盘须先在 Settings 选择对应机型。

## 2026-10-09 readonly2：首次真机反馈后的修正

用户报告 PERF 日志阶梯状错行、约 30 FPS、audio_missing 约 22000/2s；打开 Model_4 内 GOBBLER.JV3 等镜像黑屏，但日志持续输出。其他入口能够进入，未确认完整游戏验收。

源码确认旧版目录仅分组，启动仍用 Settings 默认 Model I。主机同一 GOBBLER.JV3 用 Model I 运行约 32 秒显示空白（pc=06c2）；用 Model 4 出现 LDOS 5.1.4 Date 提示（pc=3399，64×16）。该镜像运行在兼容显示模式；LS-DOS 6.3.1 才进入原生 80×24。此证据定位机型错配，实际设备黑屏仍需复测。

新候选 trs80-readonly2：已知机型目录及其子目录自动选择对应机型、BIOS、运行时渲染和虚拟键盘；普通目录保留设置默认值。目录标签显示 Model I / Model III / Model 4，CMD 为独立程序，JV1/JV3 为磁盘镜像。增加 LOAD 日志核对实际机型与路径。

原始 UART PERF/heartbeat 行尾使用 CRLF。旧 draw 包含 compat_framebuffer 等待；新版拆出 wait。渲染从逐像素除法和字形取数改为逐源扫描行生成像素，再保持原有水平区间覆盖缩放，没有取消显示 READY 保护。

16/16 CTest 通过，包括新增 Gobbler4 的日期提示/64列启动断言；768 组新旧渲染器逐像素差分通过（机型、扩展宽度、字形模式、反色、视频页、缩放尺寸、扫描线、stride 留白）。两种 HW16 UF2 构建通过，RAM 455548 B，FLASH 268016 B。哈希与大小见 build-evidence.json。

H: 序列号 7AD1442C 再次确认，旧 TRS80 内容先备份，readonly2 写入 H:/emu/16/trs80.uf2 并回读校验；18 份媒体/通知回读校验，67 个其他固件和其他索引行不变。报告与旧文件备份在 trs80/build-deploy-H/20261009-165709/。尚未执行设备烧录或宣称黑屏/60 FPS 已验收。
