# RetroPicoES v2.1 S17-B 内存布局与确定性回放（noiz2sa）

S17-B 用固定上游头文件测量引擎热/冷对象规模，并以脚本化回放证明核心逻辑在
宿主上可确定性重放。

## 测量证据（`noiz2sa_meminfo`，宿主实跑）

```text
foe pool:      98304 B (FOE_MAX=1024 sizeof(Foe)=96)
shot pool:     192 B (SHOT_MAX=16 sizeof(Shot)=12)
ship:          24 B
palette:       512 B (256 x RGB565)
index layers:  768000 B (l1/l2/pbuf=153600 B each, panels=76800 B each)
rgb565 mid:    614400 B (640x480 mid buffer before downscale)
NOIZ2SA MEMINFO: PASS
```

`FOE_MAX=1024` 与 `FOE_TYPE_MAX=4` 是 `foe.cc` 的私有常量，端口侧镜像并在源码
注释中标注同步要求。

## 目标端布局（S17-C/S17-D 落地）

| 区 | 对象 | 规模 | 归属 |
|---|---|---|---|
| PSRAM | 索引层 l1/l2/pbuf | 各 153600 B | `compat_large_malloc` |
| PSRAM | 左右侧栏 lpanel/rpanel | 各 76800 B | `compat_large_malloc` |
| PSRAM | RGB565 中间缓冲（下采样输入） | 614400 B | `compat_large_malloc` |
| PSRAM | OGG BGM 解码环形缓冲 | 预留 1 MiB | S17-C 定稿 |
| PSRAM | BulletML 解析树（73 个 XML） | S17-C 实测 | 按需分配 |
| SRAM | foe 池 | 98304 B | 静态热数据 |
| SRAM | shot 池 / ship / 调色板 | 192 / 24 / 512 B | 静态热数据 |
| SRAM | 下采样行缓冲与临时 | < 4 KiB | 热循环 |

- SRAM 预算：静态热数据合计约 98.9 KiB，叠加 compat/驱动既有占用后仍明显低于
  其他端口 85% 上限；S17-D 以 HW16 构建的静态 RAM 报告复核。
- PSRAM 预算：上表 PSRAM 项合计约 2.4 MiB（含 1 MiB 预留），低于 8 MiB 的一半。

## 确定性回放证据

- 场景：标题按 Start → 进入第 1 关 → 持续开火并以 90 帧为周期做左右微移，
  共 1800 帧；结束时 `status=1`（IN_GAME）、`score=2000`，说明射击、命中、
  计分与 BulletML 弹幕推进都进入了摘要。
- 判决：两次独立工作目录的完整回放摘要完全一致。

```text
NOIZ2SA STATE DIGEST c512aacd52187576 tick=1801 status=1 score=2000
NOIZ2SA HOST SMOKE: PASS frames=1800
```

- 摘要覆盖：状态机、tick、interval、score/left/stage/scene/endless/insane、
  bonusScore、ship 全字段、16 发 shot 池、foeCnt 与 enNum[4]。
- 确定性来源：shim 每次 `SDL_GetTicks` 固定步进 16 ms，`srand(SDL_GetTicks())`
  种子链因此固定；输入由 `noiz2sa_port_set_input_hook` 每帧锁存。

复跑命令：

```powershell
cd noiz2sa
.\scripts\test-determinism.ps1     # 两次隔离回放并比对摘要
.\build-host\noiz2sa_meminfo.exe   # 内存规模与预算断言
```
