# RCA Studio II 候选交接

任务：RCA-STUDIO2；基线：7330ed6；分支：codex/rca-studio-ii。
状态：源码/主机/构建候选完成，真机未验收；性能公共层存在合同缺口。
修改范围只有 studio2/**；公共文件、中央状态、其他端口和主工作区未修改。

## 自动验证

- GNU 14.2，C11，核心与生产 frontend 使用 `-Wall -Wextra -Wpedantic -Werror`。
- CTest 12/12：核心、生产适配器、公共 ROM 浏览器、Settings 宿主、生命周期及菜单。
- 核心：ADC/SDB/SMB 全部 256×256×2 算术组合；256 opcode 执行/周期烟测，不把烟测称作完整逐指令语义证明。
- 分支跨页、长分支、移位、MARK/RET、IO、IRQ/IDL、DMA 地址回绕。
- BIOS 尺寸、原始卡带超限、ST2 重复/非法页/截断拒绝与失败后状态不变。
- 原创诊断经实际 CPU、中断和 DMA 运行 120 帧，两台机器逐字节一致；条纹像素、采样总量、输入门控音频通过。
- 连续音诊断验证正负样本与约 1 秒内 600 次符号变化，对应 300 Hz。
- RGB565 中心/边缘/stride 守卫，按键 bank、菜单 Home、设置校验。
- 生产 frontend 的缺 BIOS、短 BIOS、超限卡带、无效 ST2、音频初始化失败及暂停/恢复/停止/Menu 交接。
- 公共前端采用检查 PASS；没有私有 ROM 页面、五项动作菜单或旧组合手势。
- 模拟 SD 安装测试：两次安装保持唯一索引，ROM/存档哈希不变，拒绝 standalone UF2 与满 32 项索引。
- 原创封面 PNG 320×240 已本地查看。

## 固件

相对本 worktree 的路径：

|模式|UF2|地址|大小|SHA-256|
|---|---|---|---|---|
|Loader|studio2/build-pico-retropicoes-v21-bootloader-make/studio2.uf2|0x10080000|395776 B|957E01A2C3BD7641133E04D358A51C570B345E5554515AD45026DE99FD0E7877|
|普通版|studio2/build-pico-retropicoes-v21-make/studio2.uf2|0x10000000|395776 B|A14297997718D8CF4072DF4687D49450B67EA0F62788724707AF85EA7FE15419|

Flash 197600 B；主 RAM 404228/524288 B（77.10%）；Scratch X/Y 各 2048 B；PSRAM 无核心分配。
Loader ELF 的 flash 起点 0x10080000，终点 0x100b03e0；堆边界约 120 KB 余量。
UF2 的高地址元数据块来自 Pico SDK，安装门槛检查完整 Loader 应用分区；不将最高元数据地址当实际代码尺寸。
构建产物和原始日志不提交 Git。

## 待主窗口处理

共享 `digger/platform/retropicoes/perf_report.h` 第 61 行写死 1000000 us，没有窗口参数；第 6 节要求 2000000 us。
本端口链接其现有非阻塞队列/统一字段，未复制报告器、未修改共享层。10 秒心跳复用同一队列。
需要公共层唯一修改者增加可配置窗口、同步共享测试，再重建本候选。当前不得标记性能合同 PASS。
恢复游戏后音频计数重置，菜单时间不进入新游戏报告窗口。

2026-10-06 已按用户授权下载并部署作者仓库 BIOS 与 10 个卡带，来源/大小/哈希见 DEPLOYMENT_RESOURCES.json。真实游戏主机冒烟通过，但不能宣称完整游戏兼容性或真机验收完成。
两份原创 .s2d 是独立 boot diagnostic，不是 BIOS 替代品；不能用它们证明商业解释器全部正确。
音频是 300 Hz Q 门控近似，尚未用实机确认模拟音色；CPU WAIT/CLEAR 引脚模式、ELF 面板、Studio III/Visicom 彩色和状态槽不在本次实现范围。

## 集中真机验收（硬件所有者执行）

1. 确认实际 SD、Loader 与共享 PSRAM/IRQ 启动链；发现停在共享 guard 前时按并行规范交回主窗口，不改时钟绕过。
2. 生成并放入两份原创 .s2d，确认真实 ROM 选择、A 打开/B 返回、目录翻页和位置恢复。
3. 空目录仍有公共模拟游戏项；选择后明确提示无 ROM；ROM 页 Menu 进入公共 Settings。
4. 输入诊断显示稳定条纹；Start=数字 1 时发声，松开静音。Tone 诊断持续发声。
5. 游戏内 Home 打开同一 Settings，停音；恢复后图像/声音恢复。测试 SAVE/CANCEL/DEFAULT、Bank/Player 和 Controller Test。
6. Reset 二次确认；Quit 返回 ROM 页面；Return Loader 二次确认并实际返回。两个 USB 控制器各验证 A/B 键盘。
7. 补充有效 2048 B RCA BIOS 和至少两个行为不同的真实卡带，记录内容哈希、画面、输入和声音。
8. 公共性能窗口修订后，采集统一 2 秒 PERF/PERF IO 与 10 秒心跳，核对约 59.978 逻辑帧/秒及扫描端约 60 Hz。

以上步骤和四类菜单截图/记录全部完成后才更新中央 PASS。本窗口已按用户授权写 H 盘并回读校验；未烧录 BOOTSEL、未打开 COM11、未宣告硬件通过。

## H 盘部署记录（2026-10-06）

- 用户明确授权本窗口部署固件与诊断、自动下载 H 盘缺少的内容。
- H: exFAT，Volume ID b7c2e142-bc25-11f1-9852-50c2e8f4a156。
- Loader 固件 H:\emu\16\studio2.uf2，SHA-256 与上表一致；封面 H:\emu\assets\themes\0\studio2.png。
- emulators.txt 唯一项 studio2;studio2;RCA Studio II；写前已在本地 build-deploy-backup-20261006-163744 备份当时索引。
- 原创 Input/Tone 两份 .s2d 位于 H:\roms\STUDIO2；游戏位于其 Games 子目录；BIOS 位于 H:\bios\studio2.rom。全部安装后独立 SHA-256 回读一致。
- 下载源固定 ajavamind/rca-studio2 commit 7285fcead99fbfb718457dd4681c2453da44ce7e；许可明确排除 BIOS/游戏镜像，资源只保留在本地测试目录与用户 SD，不纳入 Git。
- 10 个卡带各运行双实例 600 帧，通过整机状态一致性、显示开启、非零图像和音频检查。结果见 RESOURCE_SMOKE_TEST.txt；不能据此判定游戏操作与音画时序完全正确。现有 CTest 12/12 PASS。
- **BIOS 兼容性限制**：部署的是作者公开的模拟器版 BIOS，003E 为 38 (SKP)。Emma 02 原始 Intel HEX 转出的 2048 字节 ROM 四个 512 字节片段均匹配 MAME，但 003E 为 34 (B1)，在当前核心实际卡带探针中产生黑屏。未修改原始 BIOS 来伪装校验通过；该问题尚未修复，当前候选只在公开模拟器版 BIOS 下通过游戏冒烟。两份 BIOS 原始下载与探针均留于本地 ignored build-downloads。
- 共享性能报告 1 秒/要求 2 秒的既有缺口仍待公共层修改者解决。
## Tennis 反馈与 BIOS 结论更正（2026-10-06）

用户真机反馈：Up 后出现白色球场框，实体 B 后出现左侧球拍，之后画面不动；Back 能打开 Settings。未获取当时的 Player/Bank/SAVE 状态，不能将其直接标为核心死机或确认故障已修复。

新增 studio2_tennis_probe，使用外部真实卡带，执行 A1 → B4 → A7 后观察 600 帧，并以 B2 持续输入对比无输入机器。公开作者版和未修改原始 BIOS 都通过：启动后 600 帧中各 74 次画面变化；B2 输入产生区别于无输入对照的画面结果，双实例整机状态一致。既有 CTest 12/12 PASS。

**更正上一节关于原始 BIOS 的黑屏判断**：之前通用冒烟只发送 A1、A5，没有按 Tennis 要求发送 B4/A7，黑屏不能据此证明原始 BIOS 不兼容。补全序列后，四芯片 SHA1 匹配 MAME 的原始 BIOS 也可运行 Tennis。此前“未修复 BIOS 黑屏”的解释证据不足；保留历史记录并以本次针对 Tennis 的验证为准，不将此结论推广到全部卡带。作者版 BIOS 003E 为 SKP 的字节差异仍属真实事实，H 盘部署资源未在本次更换。

当前优先人工验证上述完整输入序列，每次设置修改选择 SAVE。若用户按完整序列后仍不能启动，再根据 Controller Test 的物理键映射、实际 Player/Bank 与双手柄分配继续查前端。H 盘当前未挂载，本次未写卡、未更新固件、未更新中央验收记录。
## 输入路由 v2 诊断候选（2026-10-06）

用户继续反馈所有按键（含 Start）不改变 Tennis 画面，但 PERF/HEARTBEAT 持续。日志没有按键 mask，不能从 fps 或 input 耗时判断输入已到核心。末尾 draw 从约 6.88 ms 上升到 13.08 ms、over=30/61；需真机复测其触发条件，未将性能波动当作游戏停住的根因。

确定的端口缺陷：frontend 用 usb_name[0]/[1] 非空判断两个 USB 手柄；底层 usb_source 无论连接状态都先赋 GamePadName，因此断开设备留下的名称可能触发双手柄模式，忽略单手柄 Player 配置。修复为 usb_connected_mask 的两个连接位同时有效时才启用双手柄分配。仍无法凭现有用户日志确认该缺陷就是本次故障根因。

生产 frontend 回归覆盖单手柄连接 USB1/USB2（两名字均非空）、两手柄、NES/compat 合并输入、按键释放、累计输入观测。CTest 12/12、Tennis 双 BIOS 完整启动/输入对照、公共合同检查、安装脚本模拟 SD 测试通过。140 个冻结共享依赖 SHA-256 未变。

新增每秒 INPUT STUDIO2 v=2 记录，复用公共非阻塞 UART 队列，队列忙时推迟；没有每帧 printf。raw/src/usb 显示前端逻辑按键与连接状态，player/bank/A/B 显示实际数字键盘分配，seen/raw_seen 是本次游戏会话累计出现过的按键（捕获短按），events 为状态变化次数，ef_cycles 为被选数字对应 EF 线有效的模拟周期累计数（不等同 BIOS 分支读取次数），pc/frame/video 为核心执行位置、帧数与视频 FNV-1a 哈希。计数在游戏重置/重新加载时清零，菜单暂停不推进模拟周期。

新固件（397824 B，构建已完成，H 盘未挂载，**尚未部署**）：

- Loader：build-pico-retropicoes-v21-bootloader-make/studio2.uf2，SHA-256 B28297EC0B32496122F3CB68F2301C9E85D7FB43B249013074E9A082B599B411。
- 普通版：build-pico-retropicoes-v21-make/studio2.uf2，SHA-256 0F411AE18B3E06636D83C56A24A93CDFF326C2477D60AAF951B1F628DAADF9EA。
- Loader Flash 起止 10080000–100b0750；主 RAM 终点 20062348，堆上界 20080000。

下一项最小真机验证：部署本候选确认串口出现 INPUT STUDIO2 v=2，进入 Tennis 后先无操作 2 秒，再分别按住 Start、Up、Left 各 2 秒并松开，连同当前 Settings 的 Player/Bank 值提供输入日志。先用原始键 → 数字键盘 → EF 有效周期 → 视频状态逐层定位；不要求用户反复尝试启动序列来替代输入观测。若 USB 没有按键，检查 Controller Test；若 raw 有变化而 A/B 分配不正确，查配置/路由；若已送到正确键盘而游戏仍不动，继续核心/卡带执行跟踪。真机确认前不更新中央 PASS/技能问题库。
### 输入诊断 v2 实际写卡（2026-10-06 17:41）

用户报告 H 盘已接入后，核对 Volume ID 与此前一致，备份旧固件、菜单索引与封面至 build-deploy-backup-input-v2-20261006-174126。已将上述 Loader 输入路由 v2 固件写入 H:\emu\16\studio2.uf2；独立 SHA-256 回读 B28297EC0B32496122F3CB68F2301C9E85D7FB43B249013074E9A082B599B411 一致。菜单 studio2 唯一项；13 个现有 ROM/诊断/BIOS/存档/设置文件（实际存在项）写前写后哈希一致。详情见 DEPLOYMENT_INPUT_V2.json。

本次仅完成介质部署，待用户上板提交 INPUT STUDIO2 v=2 与按键反应日志，未标记真机问题修复或中央 PASS。
## Tennis 等待设置的精确复现与快速启动 v3（2026-10-06）

用户 v2 日志显示 usb=1、player=A、bank=0、raw_seen=7f、events 持续增长、A 非零而 B=000、ef_cycles A 持续增长/B=0、PC 变化，video 始终 a1a0e485。输入已进入核心，不是 USB 完全失效，也没有证据支持 CPU 死机。

本地主机以相同作者版 BIOS 与原版卡带，先发送 A2 再 A5，得到精确相同视频 FNV-1a 哈希 a1a0e485；外框、中线、左侧球拍与用户照片一致。之后 600 帧无输入、60 帧 A2、120 帧释放均 video_changes=0。由此定位到卡带设置阶段只收到 A 键盘、等待另一套键盘输入；先前单手柄必须多次手工切换 Settings 的体验不完整。

复用公共 Settings，新增仅匹配真实 Tennis 卡带指纹的 Start Tennis 动作，二次确认后重置本局并在正常模拟帧中执行 A1/B4/A7（每次按 12 帧，启动及松开阶段各 120 帧，总 516 帧约 8.6 秒）。随后 Player B/Bank 0 单手柄 Up/Down 控制 B 球拍。本次会话设置同步到 UI，但不自动保存文件。菜单暂停冻结序列帧数；其他卡带与 .s2d 不显示该动作。保持原始 BIOS/ROM 字节不变。

自动流程通过真实 production frontend 而不只是 core 探针验证：作者版与 MAME 四芯片匹配原始版 BIOS 均通过完整序列、600 帧持续画面变化、B2 输入送达；另有误调用（未识别卡带/未暂停）拒绝验证。CTest 12/12、公共合同、模拟 SD 安装通过，140 个共享依赖哈希未变，双地址构建完成。

v3 候选 399360 B（当前 H 盘未挂载，尚未部署）：

- Loader SHA-256 9381FF1706163414E2B48B46620B85A927354DC187728461F34A7BD6CC146A96。
- 普通版 SHA-256 568E1584B480A6D428755A91E2D6DEAFFB5EAEA95ABFF407C0AFBA02DBBB4BC1。

真机下一项验证：确认 INPUT v=3，在 Tennis 的 Settings 选 Start Tennis 并确认；等待约 9 秒，核对 setup=0、两键盘 seen/ef_cycles 均出现过输入、视频哈希变化，验证 Up/Down 球拍与实际声音。未获用户确认前不标为真机 PASS。
### Tennis 快速启动 v3 实际写卡（2026-10-06 17:52）

用户报告 H 盘接入后，核对 Volume ID 与原卡一致，旧固件/菜单/封面已备份至 build-deploy-backup-tennis-v3-20261006-175236。已安装 Start Tennis 候选至 H:\emu\16\studio2.uf2，独立 SHA-256 回读 9381FF1706163414E2B48B46620B85A927354DC187728461F34A7BD6CC146A96 一致，菜单 studio2 唯一项，13 个 ROM/诊断/BIOS/存档/设置现有文件写前写后哈希一致。详见 DEPLOYMENT_TENNIS_V3.json。

待用户从 Tennis 的公共 Settings 选 Start Tennis 并确认，约 9 秒后验证上下球拍、运动的球与声音，核对 INPUT v=3/setup=0；本次写卡不等于真机验收 PASS。
## 用户确认 v3 与直接启动 v4（2026-10-07）

用户明确回复“这两项成功”，确认 v3 Tennis 经公共 Settings 的 Start Tennis 启动后，球的动态运动和上下键球拍控制正常。将这个独立子任务记录为真机已验收；未扩大为全部卡带、音频、长时间稳定性或整个前端合同 PASS。个人移植技能的任务记录 RCA-Tennis-v3 与复用问题案例已追加，保留原始失败/诊断记录。

按用户新要求，v4 在成功载入识别的 Tennis 卡带后立即安排同一 516 帧设置序列；不必先打开 Settings。运行期默认 Player B/Bank 0，公共 Settings 的工作副本与 dirty 判断均以实际生效控制配置为基准；用户手动 SAVE 的设置仍受尊重，自动默认不写磁盘也不泄漏到其他卡带。Start Tennis 动作用于重开，仍有确认。Menu 暂停/恢复冻结设置进度。

验证：12/12 CTest，真实 production frontend 分别使用作者版与 MAME 匹配原始 BIOS，从默认 Player A/Bank 1 打开 Tennis，自动得到 Player B/Bank 0/516 帧设置，完成后 600 帧持续画面变化、B2 输入正确；测试设置期间暂停、游戏内重开、后续 .s2d 恢复原始控制配置。公共合同、模拟安装测试、140 个共享依赖冻结均通过；双地址 Release 构建成功。

v4 固件均 399360 B：Loader SHA-256 09D02903DEBAD34123DCC6793B3681736C7785BA00E10A04CC324E5AACDD5A67；普通版 SHA-256 80F920B928BC1690E392B4FE276D887E14210EC4EF1275B08DB730C68CFDDEA7。

检测到原 H 盘重新出现后已按持续授权部署 Loader v4，旧固件/索引/封面已本地备份；安装后独立哈希一致，13 个 ROM/诊断/BIOS/存档/设置文件哈希未变，菜单 studio2 唯一项。详情见 DEPLOYMENT_TENNIS_V4.json。v4 直接从 ROM 列表启动的真机体验待用户复测，不借用 v3 的 PASS。

## 绘制优化 studio2-perf1（2026-10-07）

用户提交 v4 真机日志：59.9 FPS，step 约 3.45 ms、draw 约 6.88 ms、total 约 10.60 ms，over=0/60，音频欠载及音视频丢弃均为 0。INPUT 显示 setup=0、Player B/Bank 0，按键与视频哈希变化；日志本身不替代球拍、声音等人工体验确认。绘制占总耗时约 65%，本轮针对绘制增加预算余量。

studio2_compose 改为每行读 8 个视频字节，每个源像素判断一次并连续写 5 个目标像素，上下黑边按行清零。源行 floor(y*128/192) 等价简化为 floor(y*2/3)。保持 320x240 输出、320x192 居中内容、RGB565、扫描线、stride 与半字对齐缓冲区行为；CPU、音频、v4 输入及 Tennis 自动启动流程未变。启动标识改为 studio2-perf1。

验证：新增 16 种视频图案 × 2 种 stride × 2 种扫描线共 64 屏逐像素旧算法对照，含黑边、padding、首尾保护字和仅半字对齐缓冲区；CTest 12/12、公共合同、模拟 SD 安装（含独立版地址拒绝）通过。双地址 Release 构建成功，冻结共享依赖未修改。Loader 主 RAM 终点 0x20062354，堆上界 0x20080000，Flash 终点 0x100b0ba8。

两份固件均 399872 B：

- Loader：build-pico-retropicoes-v21-bootloader-make/studio2.uf2，SHA-256 A025A428E063AE5B94042A4E6D4CE09EE5A0627403EEF431C9F9B175611D41FB。
- 普通版：build-pico-retropicoes-v21-make/studio2.uf2，SHA-256 346876FFE76221569C6C72F7B77506C26662C599362A9D947B9CB12FC0826298。

当前未检测到 H 盘，尚未写入实体 SD。真机复测应确认启动标识 studio2-perf1，使用同一卡带与扫描线配置，对比 draw/total/peak 与音视频计数，并确认画面、上下控制与声音。未将主机等价测试或构建标为性能真机验收。既有公共层 1 秒性能窗口与要求 2 秒的缺口仍保留，未修改冻结共享层。

### studio2-perf1 实际写卡（2026-10-07）

用户报告接入后核对原 H 盘 Volume ID 一致，备份旧固件、索引、封面至 build-deploy-backup-perf1-20261007-093644。只更新 H:\emu\16\studio2.uf2，分阶段及安装后 SHA-256 均匹配上述 Loader 候选。15 个受保护文件（索引、封面及 13 个 ROM/BIOS/存档/设置文件）写前写后哈希一致，菜单 studio2 唯一项。详情见 DEPLOYMENT_PERF1.json。等待新候选真机 draw/total/peak、画面与控制反馈，尚未标记性能验收。

## RCA-Tennis-Perf1 · 已测启动、控制及性能验收（2026-10-07）

用户提供 INPUT v=4 / STUDIO2 真机日志，并在询问“从游戏列表直接打开 Tennis 后，无须进入 Settings，球会运动、上下键能控制球拍吗”后回复“可以控制”。结合自动序列结束、画面持续更新及此前已确认球运动，将当前已测 Tennis 的直接启动、球拍控制和稳态性能记为 PASS；未将可听音效、其他卡带或长期稳定性推定为通过。

- 版本：studio2-perf1，INPUT v=4；RP2350B / HW_CONFIG=16、252 MHz、Loader 应用。当前本地 Loader SHA-256 与前述部署记录一致。
- 本段六个窗口逻辑 FPS 均 59.9；显示和物理 refresh 多为 59.9，其中一窗同为 60.9。不是所有窗口显示精确 60.0。
- input 231–238 us，step 3445–3465 us，draw 1582–1586 us，total 5286–5315 us，peak 5349–5393 us；budget 16673 us，全部 over=0/60。平均计算余量约 11.36 ms。
- audio_under/audio_drop/video_drop/resync/uart_drop 全为零。只证明本段供给和传输计数正常，不替代听感确认。
- setup=0，Player B/Bank 0；按下 raw=08 时 B=100，松开时 B=000；events 20→40，B 的 EF 有效周期增加，六窗 video 哈希均变化。输入通路与活动画面均有日志证据。
- 对照此前 v4 日志 draw 约 6883 us、total 约 10611 us，当前约 1585/5300 us，绘制降低约 77%、总计算耗时降低约 50%；step 仍约 3.45 ms。与逐字节扩展视频像素的改动方向一致，不归因于 CPU 加速。
- 已有验证：64 屏旧/新缩放逐像素对照、12/12 主机测试、双地址构建和 SD 写入回读。收到本次反馈后再次运行当前 build-host 的 12/12 CTest，均通过；本次仅更新记录，未重新构建或写卡。

个人移植技能记录与 FPS 案例以 RCA-Tennis-Perf1 追加。原 v3 验收及 v4/perf1 候选待测历史保留；中央任务及公共层合同缺口不由本次受限验收自动更改。
