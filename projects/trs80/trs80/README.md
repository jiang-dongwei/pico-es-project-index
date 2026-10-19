# TRS-80 Model I / III / 4 — RetroPicoES

任务 TRS80；分支 `codex/trs80-model-i-iii-4`；基线 `7330ed6`。
工作树：`E:/ComporyProject/pico_ES_Public_Projects/Pico_ES_TRS80`。

当前候选 `trs80-readonly2` 已形成 HW16 普通启动和 Loader UF2，尚未真机验收。
采用固定 xtrs Z80 和字形数据；支持三机型 BIOS、普通 CMD、只读 JV1/单头块 JV3，
Model 4 原生 80×24 显示、128 KiB 分页、字符/半图形、视频分页及反色。
公共 ROM 浏览器、同一 Settings 容器和生命周期均已接入。

```powershell
python .\trs80\scripts\prepare-runtime-data.py
.\trs80\scripts\test-host.ps1
python .\trs80\scripts\prepare-test-package.py
```

命令在本工作树根目录执行。需 CMake 与匹配 DLL 的 MinGW 工具链。
无外部媒体时有 9 项主机测试；下载媒体后共 16 项，包含三机型 BASIC READY、
LDOS/LS-DOS 日期提示及 Model 4 原生显示/内存模式断言。

固件构建使用 `scripts/build-retropicoes-v21.ps1`，指定 Pico SDK 2.3.0、
Pico-PIO-USB 和 tusb_xinput 路径；`-Bootloader` 构建 Loader 应用。
构建目录分别为 `build-pico-hw16-loader` / `build-pico-hw16-standalone`。
两个 UF2 不可互换：Loader 应用地址为 `0x10080000`，普通启动为 `0x10000000`。

产品目录：BIOS `/data/trs80/bios/model1.rom`、`model3.rom`、`model4.rom`；
媒体 `/roms/TRS80/`；设置 `/settings_TRS80.dat`（SAVE 才持久化）。
本地 `build-sd-test-package` 按机型分目录提供已下载测试媒体和原始通知，
仅用于当前测试，不代表商业分发授权。Model_I / Model_III / Model_4 文件夹会自动选择对应机型；其他目录使用 Settings 的机型。
为明确格式，浏览器只列 `.CMD`、`.JV1`、`.JV3`；包内 `.dsk` 已按真实格式改名。

手柄：方向为原机箭头，实体 A 发 SPACE、B 发 ENTER；Menu/Home 单键打开 Settings；
Start 打开原机矩阵虚拟键盘，默认 ENTER，A 发送一个按下/释放脉冲，B 返回。
键盘选择期间暂停 CPU/音频，发送后恢复。SHIFT 可选择；机型专属键按型号显示。
CMD 冷启动先完成 BIOS 的通用键盘初始化，再按 CMD 入口执行；不按游戏名称适配。

Loader 安装器 `scripts/install-trs80-bootloader-sd.ps1` 校验地址/家族，保留其他索引项，
删除重复 TRS80 行，备份原索引并验证文件哈希；可用 `-AllowDirectoryRootForTest`
在临时目录验证。测试包和安装器均未对真实 SD 或设备执行过写入。

来源与许可见 `doc/CORE_REFERENCES.md`、`doc/RUNTIME_MEDIA.md`；
当前验证、限制与下一步见 `doc/HANDOFF.md` 和 `PORTING.md`。
