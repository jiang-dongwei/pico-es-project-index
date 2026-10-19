# 本地测试资源（2026-10-09）

用户明确授权在本地没有资源时由本任务下载 BIOS、游戏和磁盘镜像。
全部保存在本工作树 `_deps/runtime-data/`；原始 ZIP、ZIP 许可注释和清单留存，
未纳入 Git、未写 SD 卡。`scripts/prepare-runtime-data.py` 固定 URL 与 SHA-256，
重复执行复用已通过校验的下载。明细哈希见该目录 `inventory.json`。

| 内容 | 来源与使用依据 | 本地位置 |
|---|---|---|
| Model I Level II v1.3 | [RetroStack ROM 仓库](https://github.com/RetroStack/TRS-80-Model-I-System-ROMs)，固定提交 `3f31faebde2214214acb74f87f06b836df3914d2`；仅本地测试下载；未确认商业分发许可 | `bios/model1.rom` |
| Model III / 4 兼容 BIOS | [xtrs 官方 MODELA/III 下载](https://www.tim-mann.org/xtrs.html)，对应 LS-DOS 6.3.1H；保留配套 ZIP 中关于 Microsoft BASIC、Tandy 与 Frank Durda IV 代码的完整说明，不将它泛称为开源 BIOS | `bios/model3.rom`、`bios/model4.rom` |
| LDOS 5.3.1 I/III 系统盘 | [Misosys](https://www.tim-mann.org/misosys.html)；页面与 ZIP 注释记载 Roy Soltoff / William Schroeder 的免费使用及保留通知的传播许可 | `roms/ld1-531/`、`roms/ld3-531/` |
| LS-DOS 6.3.1H Model 4 系统盘 | 同上，包含具体例外说明，已保留 ZIP 注释 | `roms/ld4-631/` |
| Frogger、Castle、Scarfman、Painter、Bounce | Misosys 的 `Cornsoft.zip`，ZIP 注释载明 Roy Soltoff 取得版权并允许免费使用与保留通知的传播 | `roms/Cornsoft/` |
| Checkers 与其他 Lance 软件 | Misosys 的 `Lance.zip`，同一通知条件；BAS 文件当前尚无磁带/BASIC 装载适配 | `roms/Lance/` |
| Gobbling Box I / III/4 | Misosys 的 `gobbler.zip` / `gobbler4.zip`；保留 ZIP 的许可通知 | `roms/gobbler/`、`roms/gobbler4/` |

Model I 的两片 8 KiB padded 镜像合并为 12 KiB（A 的 8 KiB + B 的前 4 KiB）；
MODELA/III 按 CMD 记录解析为 14 KiB raw BIOS，不把 CMD 容器直接作为 raw ROM。
Model III 与 Model 4 使用相同兼容 ROM 并非原生 Model 4 启动已完成。

本轮宿主证据：三种机型经启动输入后均出现 `READY`；Model I 的 Frogger 与 Castle
执行约 5 秒无 CPU fault，并在字符视频内存输出中出现其标题。
游戏图形、键盘实际操作、音频、保存与 Model 4 原生系统盘均未验收。
已下载磁盘镜像尚不能运行；FDC 未实现。

部署门槛当前为 `HARDWARE READINESS: BLOCKED`（缺完整固件与磁盘/音视频适配）。
本地下载校验成功不等于 SD 的 `CONTENT INSTALL: PASS`；不得通知弹卡上板。
