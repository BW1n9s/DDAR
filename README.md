# DD Appearance Replacer — MCM 测试版

当前版本 **0.0.3** 包含真实 SkyUI MCM、ESL 标记 ESP、三个 PEX 和 SKSE DLL。菜单连接背包 A 选择、DD／DCL 目录 B 选择、完整槽位诊断、应用、停用与恢复。游戏内效果尚未验收；不是正式 DDAR v0.1。

A 提供外观，B 保留 DD 设备身份。B 来自实际加载的 DD NG 设备数据库，不要求在背包中。多槽位按 Rendered Armor 完整槽位集合筛选；部分相交不会自动视为可兼容。

- [安装和游戏验证步骤](docs/mcm-test.md)
- [构建验证及复现命令](docs/mcm-build-validation.md)
- [设备目录与多槽位设计](docs/mcm-device-catalog.md)

测试包：`F:\DDAR-Dev\Packages\DDAR-MCM-0.0.3-test.zip`。在 MO2 作为独立 Mod 安装并启用 `DDAR.esp`，通过 MCM 操作，无需编辑 INI、输入控制台命令或使用快捷键。已有游戏、MO2 Mod、DD 配置和同步参考文件均未修改。

## 当前范围

五页菜单用于实际 A→B→恢复验证。暂限一组随存档保存的选择、女性玩家、SE 1.5.97、本机已核查 DD NG DLL 和单 ARMA 腰带。加载存档后保持停用，需手动应用。外部 JSON、多组映射、中文界面、AE 和通用多 ARMA 后端仍需实现。

`Applied` 表示后端接受了映射，不代表已证明显示正确。外观、恢复、DD 锁定／钥匙、物理和 NPC 隔离必须在游戏里检查。旧版 Phase 0 文档保留为历史记录，当前行为以 MCM 测试指南为准。

## 源码与构建

实际开发目录：`F:\DDAR-Dev\Workspace\DDAR`。开发工具、依赖、缓存和产物置于用户确认的 `F:\DDAR-Dev`。

- `src/Probe.cpp`：SKSE 入口、接口注册、装备和 Biped 日志。
- `src/McmBridge.cpp`、`papyrus/`：菜单、设备目录、关闭暂停菜单后执行的应用／恢复。
- `src/Experiment.cpp`：本机二进制保护下的玩家 ARMA 替换实验。
- `tools/Build-Probe.ps1`、`tools/Build-Mcm.ps1`：DLL、PEX、ESP 和 SEQ 构建。
- `tools/verify_mcm.py`、`tools/package_probe.py`：产物接口检查和测试包生成。
- `third_party/DDNG/`：带 Apache-2.0 许可的 API 头文件；不包含 DD 游戏资源。

已通过原生编译、三个脚本编译、ESP 回读、跨语言接口与包检查，以及 16 项 Python 检查和 8 项原生槽位检查。游戏验收尚未完成。
