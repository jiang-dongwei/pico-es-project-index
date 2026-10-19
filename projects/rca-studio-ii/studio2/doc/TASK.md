# RCA Studio II 开发边界

任务：RCA-STUDIO2；分支：codex/rca-studio-ii；基线：7330ed6。
允许修改：studio2/**、.emv2/discussion/rca-studio2/**、该任务专属checkpoint。
禁止修改：common/**、compat/**、其他端口、中央.emv2状态、Loader和硬件。
独立worktree，不合并main，不烧录，不写SD，不占COM11。

|能力|实现目标|
|---|---|
|ROM浏览|公共浏览器，ST2/BIN/ROM和子目录，空目录模拟项|
|Menu入口|物理Menu/Home单键|
|退出/重置|公共生命周期，退出回ROM页，重置CPU/机器|
|状态存读|首版不提供完整存读，不显示无效菜单项|
|视频|CDP1861 64×128物理行，Studio II BIOS复用行形成64×32；RGB565|
|音频|Q门控300Hz蜂鸣音，HDMI链|
|专属设置|数字键盘Bank 0/1、目标键盘A/B|
|持久化|每模拟器独立设置，SAVE提交|

开发以源码、主机测试、双地址构建和整体真机验收四层交接。
