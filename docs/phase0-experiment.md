# Phase 0：单组外观替换实验（0.0.2）

这是可编译的渲染实验，**还没有游戏实测，不是已验收的 DDAR v0.1**。
没有 MCM、JSON Registry、自动读档应用或高级效果。先验证真实换模闭环，再扩展界面。

## 本次接入方法与边界

已对本机 1.5.97 EXE、Address Library 和实装 DD NG DLL 做只读核对。
DD NG 导出的 `GetAPI` 返回 API v2；其前四个虚函数位置与本机二进制匹配。
调用配对查询前核对 DLL 完整 SHA256、导出位置、vtable 和版本。
先从 Rendered 反查 Inventory，匹配后才正查。原因是本机正查函数对未知键会插入数据库项。
不读取跨 DLL 的 STL 容器，也不把 Keyword 当作完整 DD 身份。

实验在引擎 ARMA 构建入口选择 A 的 ARMA，传入的 Armor 仍为 B Rendered。
只对与玩家第一／第三人称 Biped 指针匹配的调用生效；NPC 原样转发。
保留 DD Hider 的上游判断，不覆盖其补丁区域。
没有写入共享 Armor/ARMA 字段，不调用装备、卸除、钥匙或解锁功能。
首次启用前检查 EXE 完整 SHA256、Address Library 解析结果和入口字节；
入口已被其他插件修改时拒绝安装 hook。当前也拒绝加载了 DynamicArmorVariants 的情况。

关闭实验后，hook 保留为透明转发，随后模型重建选择 B 原 ARMA。
恢复使用本机 DD Hider 同样调用的装备 3D 刷新入口，不通过重穿 DD 设备实现。
`DISPATCHED` 日志只表示引擎收到了替换参数，不代表角色已经正确显示模型。

## 测试对象

提供的配置针对本机文件指纹：

| 对象 | 插件 + 本地 ID |
|---|---|
| A | `[Predator] MagicWand Belt.esp` + `000D63`，MagicWand Belt Leather Black |
| B Inventory | `Devious Devices - Integration.esm` + `009A7B`，金属贞操带（衬垫） |
| B Rendered | `Devious Devices - Assets.esm` + `00435D` |

这些不是完整运行时 FormID。只有 A 需要在玩家背包，A 不必穿戴。
B 从设备目录解析，不要求在背包中；登记映射不添加或装备 B。
DLL 在启用时用实装 DD NG 数据库再次核对 B 配对，并要求最终覆盖 Armor/ARMA 的插件与核查夹具一致。
仅支持女性玩家、单个 ARMA，以及通过种族和模型资源可读取检查的腰带。
模型资源可打开不代表 NIF、材质、骨骼或物理已验证。

**关键限制：A ARMA 为槽位 49，B ARMA 为 49、52。**
默认严格策略拒绝这组，不静默忽略差异。测试者可以显式设置
`AllowExtraTargetSlot52=1`，只放行这一特定差异；其他槽位和多 ARMA 仍拒绝。
这个开关是尚未验证的实验例外，不能称为“安全兼容”。必须检查旧槽位残留、身体遮挡和恢复。
它不会修改 B 的槽位。如果不接受这个实验例外，保持默认配置即可，只采集基线日志。

## 测试步骤

1. 在独立 MO2 测试配置和测试存档中使用压缩包作为一个**新的 Mod**。
   不覆盖 DD、DD NG、DCL 或其他现有 Mod。包里只有本项目 DLL、默认关闭的 INI、说明和许可证。
2. 默认 `EnableExperiment=0`、`AllowExtraTargetSlot52=0`，不装 hook，也不替换外观。
   进入游戏，取得 A 与 B Inventory，用原生 DD 流程穿戴 B，先确认锁定及交互菜单正常，截图。
   关闭暂停菜单，按 **Ctrl+F8** 记录背包与 Biped 基线。
3. 要实际实验时，在这个**新测试 Mod** 的 `SKSE/Plugins/DDARExperiment.ini` 中，
   将 `EnableExperiment` 与 `AllowExtraTargetSlot52` 都设为 `1`。
   配置只在快捷键启用时读取，不写回文件；不要求 B 在背包，也不需要为了登记而穿戴 A/B。
4. 关闭暂停菜单，按 **Ctrl+F10**。验证通过后才装 hook 和请求玩家装备 3D 刷新。
   检查是否显示 A，截图，再按 Ctrl+F8。检查日志中的 B Armor 身份及 A ARMA。
   若有 `Experiment refused`，保留原状并提供日志，不改动 DD 文件来绕过检查。
5. 按 **Ctrl+F9** 恢复，截图，再按 Ctrl+F8。检查 B 原模型、旧节点／槽位残留、DD 交互和锁状态。
   Ctrl+F10 在已启用时也会关闭并恢复。Ctrl+F8/F9/F10 都不会吞掉其他 Mod 的按键事件，
   如果本机已有相同快捷键，请先报告冲突。
6. 只有基本循环通过，才测试切换区域、第一／第三人称、读档和 NPC 使用同一 B。
   读档前会停用实验，读档后默认关闭，需再次 Ctrl+F10；目前不承诺自动应用。
7. 在恢复状态下保存测试存档，退出游戏，再禁用这个新测试 Mod，读档检查原版 B。
   不支持游戏运行中卸载 DLL。

日志位置是 SKSE 标准日志目录中的 `DDARProbe.log`，通常为
`Documents/My Games/Skyrim Special Edition/SKSE`。
需要基线、替换、恢复三组截图和对应日志，才能判断 Phase 0 是否通过。
若刷新请求成功但没有替换分派日志，可能是该设备未进入这个渲染入口；
不能因此擅自解除 DD 隐藏、强制重穿或宣称已替换。

## 构建验证

原生 x64 Release 已编译；槽位策略有 8 个原生测试，二进制解析有独立离线测试。
这些检查只验证代码与本机绑定，不能替代模型显示、锁定／解锁、物理或卸载实测。
原生 API 的核查参考 [DD NG API](https://github.com/IHateMyKite/DeviousDevicesNG/blob/main/include/API.h)
和 [Hider](https://github.com/IHateMyKite/DeviousDevicesNG/blob/main/src/Hider.cpp)，
但所有实验启用门槛均以实际本机二进制为准；上游后续变化不会自动获得支持。
