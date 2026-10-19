# TRS80 开发边界与能力

RP2350B / RetroPicoES V2.1 / HW_CONFIG=16 / 252 MHz；公共合同 v2。

| 阶段 | 当前证据 |
|---|---|
| 核心/资源 | 固定 xtrs，15 项严格主机测试通过，三份 BIOS 与测试盘已下载并锁定哈希 |
| 磁盘启动 | Model I JV1 / III JV3 启动 LDOS；Model 4 JV3 启动 LS-DOS，原生 80×24、map≥2 |
| 视频/音频 | RGB565 字符、半图形、分页、反色；22050 Hz 周期驱动样本，core1 HDMI 环缓冲；主机测试通过 |
| 前端/输入 | 公共浏览器/Settings/lifecycle；保留目录与选择；空目录 Demo；实体 A/B；Start 虚拟键盘 |
| 固件/交付 | 普通与 Loader UF2 已构建；原创 320×240 封面、幂等安装器、本地测试包、2 秒遥测与 10 秒心跳 |
| 真机 | 未烧录、未写卡、未访问串口；画面/声音/输入/FPS/退出待集中验收 |

## 实际能力

- Settings：机型、扫描线、Audio、Controls、Controller Test；运行时额外有 Reset/ROM List；
  浏览器和游戏使用同一公共容器。Save/Cancel/Default 由公共导航驱动。
- 媒体：BIOS、普通 CMD、JV1、单头块 JV3。挂载数据由调用者保持有效，全部只读。
  坏 JV3 先验证边界；错误挂载不覆盖既有盘。写扇区/格式化返回写保护。
- FDC：restore/seek/step、read sector/multiple、force interrupt、DRQ、完成 IRQ/NMI、
  Model III/4 WAIT、磁盘缺失、数据未及时取走和电机超时状态。读地址/整轨、完整格式化、
  双头块 JV3、DMK、CAS、倍密扩展与 motor-off NMI 尚未实现。
- Model 4：64 KiB 基础 RAM 留 SRAM，64 KiB 扩展 RAM 显式绑定 PSRAM；核心不调用板级分配器。
  不将全部 128 KiB 静态放入 SRAM，以避免初版 99.35% 链接占用。
- 音频：cassette 输出与 III/4 一位 DAC；模拟线程生成，SPSC 队列跨核传递，
  暂停/停止先关闭音频消费者再清空队列。未实现 Orchestra 扩展或磁带加载。
- 状态序列化/磁盘写入/存盘未实现，因此隐藏状态菜单，不提供虚假保存动作。

## 验证边界

主机屏幕断言证明系统加载到日期提示，不证明 OS 全部命令；Frogger/Castle 标题探针
不证明实际游戏流程。生成的视频帧来自主机渲染器，不是 HDMI 真机截图。
前端完整 C++ 源码用 Cortex-M33 `-Wall -Wextra -Werror` 编译；实际控制器与音频时序待真机。

`PERF` / `PERF IO` 同窗每 2 秒报告：逻辑/目标/显示/刷新 FPS，input/step/draw/present/wait
平均耗时，整帧工作均值/峰值、16667 us 预算、超预算和核心负载；I/O 欠载/环缓冲缺样/
丢弃、视频丢帧/resync/UART 丢弃。音频生成耗时包含在 step，core1 输出标为 async。
串口为固定 640 字节队列，每帧最多非阻塞发送 64 字节；忙时丢报告并计数。
菜单/VK 帧不混入运行性能，加载不混入窗口；10 秒心跳不倾倒性能字段。

不修改 compat/common/其他端口/公共 QMI 或中央 .emv2。主窗口独占硬件并负责合并、
最终写卡和集中验收。本任务只交付可审核候选，不能凭 UF2 或主机测试关闭整体任务。

`trs80-readonly2`：PERF 和 heartbeat 原始 UART 输出改为 CRLF；wait 单独统计申请显示缓冲区等待，draw 仅统计像素渲染。机型文件夹自动选择机型，设置值作为其他目录的默认值。渲染器逐字形生成源扫描行，再做原有水平覆盖缩放；保留 READY/VSYNC 保护。768 组像素差分检查通过，实际 FPS 与音频缺样仍待真机复测。
