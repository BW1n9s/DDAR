# DDAR 0.0.3 — MCM 游戏验证包

本包已包含真实 MCM、ESP、PEX 和 SKSE DLL。菜单与应用／恢复接口已连接；尚未进入游戏验收。现有游戏和 MO2 Mod 文件未被修改。

## 安装与操作

1. 在 MO2 用压缩包安装 `DDAR-MCM-0.0.3-test.zip` 为独立 Mod，勾选左侧 Mod 及右侧 `DDAR.esp`。保留已有 SkyUI、DD NG、Address Library 和 SKSE 配置。
2. 通过原来的 SKSE 入口启动游戏，进入女性角色存档，等待 MCM 注册 `DD Appearance Replacer`。
3. 把普通外观装备 A 放在背包；打开 MCM 的 `Equipment mapping`（装备映射）。选择 `Appearance A`。列表每页 20 件，用上一页／下一页切换。
4. 选择 `Device B`。B 来自实际加载的 DD NG 数据库，包含其中注册的 DD／DCL 等设备，不要求它在背包里。若目录尚未完成，点 `Refresh catalogs`，稍后重新进入页面。
5. 多槽位 B 默认按 **Rendered Armor 完全相同的槽位集合**筛选。`Settings` 中 `Include partially overlapping B slots` 可以包含部分相交候选；这只扩展列表，不会自动放行不兼容组合。
6. 当前测试后端支持单 ARMA 腰带。测试已核查的 Predator 腰带 A（槽 49）和 DD 腰带 B（ARMA 槽 49、52）时，先开启部分相交筛选，并明确勾选 `Allow belt B extra slot 52`。此选项默认关闭。
7. 点 `Apply selected mapping`，确认目标及共享 Rendered 提示，**关闭全部暂停菜单**后执行。再打开 `Diagnostics` → `Status and full slot details` 查看执行结果和 A／B 完整 ARMO、ARMA 槽位。`Applied` 只表示已接入替换路径，外观是否正确需实际观察。
8. 让已有 DD／DCL 流程正常装备 B，检查 A 的外观、DD 身份、锁定、钥匙和解锁功能。DDAR 不添加、穿戴或解锁 B。
9. 在 `Current mapping` 点 `Disable / restore`，关闭菜单，检查 B 原始外观恢复。`Clear saved selection` 会清除 A／B 选择并排队恢复。

`Diagnostics` 的 `Write inventory / biped snapshot` 会在关闭菜单后记录背包和 Biped 信息。日志是 SKSE 常用日志目录中的 `DDARProbe.log`，通常位于“文档\My Games\Skyrim Special Edition\SKSE”，具体以本机文档重定向位置为准。

## 当前范围

- 五页：装备映射、当前映射、诊断、设置、关于。界面目前为英文，本文给出操作对照。
- 一组映射；A／B 选择和设置随存档保存，加载存档后保持停用，需手动重新应用。
- 多个 Inventory 共用同一 Rendered 时，映射会影响这些设备的玩家外观；详情和应用确认会提示。
- 玩家女性、SE 1.5.97、本机已核查 DD NG DLL；只支持当前校验允许的单 ARMA 腰带组合。其他槽位组合会拒绝，并在 MCM 显示原因。
- 外部 JSON、多组映射、中文界面、AE 运行时及通用多 ARMA 替换尚未实现，不能按正式 v0.1 验收。
- 不需要编辑 INI、输控制台命令或使用快捷键。本包不包含实验 INI。

## 验收记录

必须在游戏里逐项确认：MCM 出现 → A 列表 → B 不在背包仍可选择 → 全槽位显示 → 应用 → DD 正常装备及约束行为 → 停用恢复 → 读档后手动重新应用。编译通过、ESP 回读和 PEX 接口检查均不能替代这些结果。
