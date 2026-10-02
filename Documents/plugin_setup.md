# 插件开启清单（UE 5.8 / Identity5）

> 对应 `design_analysis.md` 第 2 轮 2.1 节。**目的**：让你在 UE 编辑器中一次点完，不出错、不漏项。
> 本清单已在本机 `G:\Epic Games\UE_5.8` 逐项核实过插件名、模块名与默认状态。

---

## 1. 操作步骤

1. 关闭 PIE（如果正在运行），打开 `Identity5.uproject`；
2. 菜单 **编辑（Edit）→ 插件（Plugins）**；
3. 在右上角搜索框逐个输入下表 A 组的「搜索关键字」，勾选对应插件；
4. 全部勾完后点 **立即重启（Restart Now）**；
5. 重启后编辑器会提示"模块缺失，是否重新编译"→ 选 **是（Yes）**，等待编译完成；
6. 编译完成后，`.uproject` 会自动写入 `Plugins` 数组，无需手工编辑。

---

## 2. A 组：必须开启（11 个）

> 搜索关键字填「插件面板里显示的名字」，建议直接用括号里的英文名搜索最稳。

| # | 搜索关键字（面板显示名） | 引擎路径（`Engine/Plugins/` 下） | 类型 | 用途 |
|---|---|---|---|---|
| 1 | `Gameplay Abilities` | `Runtime\GameplayAbilities` | 正式 | GAS 主体：技能 / 属性 / 效果 |
| 2 | `Game Features` | `Runtime\GameFeatures` | Beta | Gameplay Feature 插件机制（本项目的模块化方案） |
| 3 | `Modular Gameplay` | `Runtime\ModularGameplay` | Beta | `UGameFrameworkComponentManager` 扩展点 |
| 4 | `Common UI` | `Runtime\CommonUI` | 正式 | 大厅 / HUD / 结算 UI 与输入路由 |
| 5 | `State Tree` | `Runtime\StateTree` | 正式 | 交互点与 AI 的状态机 |
| 6 | `Gameplay State Tree` | `Runtime\GameplayStateTree` | 正式 | ⚠️ `UStateTreeComponent` / `UStateTreeAIComponent` **在这个插件里**，不是 State Tree 插件 |
| 7 | `Motion Warping` | `Animation\MotionWarping` | Beta | 翻窗 / 翻板 / 救援的位移对齐 |
| 8 | `Pose Search` | `Animation\PoseSearch` | 正式 | 姿势搜索选动画（追击急停急转） |
| 9 | `Audio Modulation` | `Runtime\AudioModulation` | 正式 | 心跳 / 追击音乐动态混音 |
| 10 | `Sound Utilities` | `Runtime\SoundUtilities` | Beta | 音效工具与 MetaSound 辅助 |
| 11 | `Gameplay Insights` | `Animation\GameplayInsights` | 正式 | GAS 调试时间线（开发期必备） |

**勾选后会自动连带开启的插件**（无需手动勾，看到它们变亮是正常的）：

| 被连带开启 | 由谁带入 |
|---|---|
| Enhanced Input | Common UI |
| Data Registry | Game Features / Gameplay Abilities |
| Gameplay Tags Editor | Gameplay Abilities / Common UI |
| Chooser、Blend Stack、Animation Warping | Pose Search |
| Niagara、Engine Asset Definitions | Gameplay Abilities |
| Animation Insights（面板名） | State Tree 依赖 Gameplay Insights |

---

## 3. B 组：默认已开启，无需操作

| 插件 | 用途 |
|---|---|
| Enhanced Input | 输入系统 |
| Gameplay Cameras | 相机（受击摇晃、技能镜头、相机混合） |
| Niagara | 特效 |
| MetaSound | 音频合成 |
| Sound Fields | 空间音频 |
| Data Validation | 数据资产校验 |
| Control Rig / IK Rig | 绑定与 IK |
| Modeling Tools Editor Mode | 灰盒建模（你工程里已启用） |

---

## 4. C 组：明确不要开启（及原因）

| 插件 | 不要开启的原因 |
|---|---|
| **Gameplay Interactions** | UE5.8 中仍标记为 **Experimental**，且硬依赖 SmartObjects / ContextualAnimation / NavCorridor（三个实验性插件）。它是"智能对象找槽位"的棋盘式交互模型，与本作固定交互点（板 / 窗 / 机位）不匹配。**误开会导致 C++ 编译报缺 `SmartObjects`**。 |
| Replication Graph | 5 人房间无收益，留作压测备选 |
| Iris | 实验性，5 人规模无收益 |
| Mass Entity 系列（MassAI / MassCrowd …） | 无大量 AI 单位需求 |
| GlobalDataLayer | **UE5.8 引擎目录中不存在该插件**（已逐目录搜索确认） |
| Water / Zone Graph / MassInsights | 与本作玩法无关 |

---

## 5. 故障排查

| 现象 | 原因 | 处理 |
|---|---|---|
| 插件面板搜不到 `Gameplay State Tree` | 名字里有空格，或者被分类折叠 | 搜索框输入 `Gameplay State`，或展开左侧 **Gameplay** 分类 |
| 搜不到 `Gameplay Abilities` | 左侧分类被过滤 | 点搜索框右侧的"全部"按钮清除分类过滤，或展开 **Gameplay** 分类 |
| 重启后 C++ 编译报 `Cannot find module 'GameplayTags'` | 误以为 GameplayTags 是插件 | `GameplayTags` 是**引擎内置模块**（`Engine/Source/Runtime/GameplayTags`），只需在 `Build.cs` 加依赖，不需要开插件 |
| 编译报 `Cannot find module 'SmartObjects'` | 误开了 Gameplay Interactions | 关掉它，重启 |
| 编译报 `Cannot find module 'GameplayStateTree'` | 模块名写错 | 正确模块名是 **`GameplayStateTreeModule`**（插件名与模块名不一致） |
| 编译报 `Cannot find module 'AnimationWarping'` | 模块名写错 | 正确模块名是 **`AnimationWarpingRuntime`**（同理 `AnimationLocomotionLibraryRuntime`） |
| 勾选后卡在 "Restart Now" 没反应 | 编辑器有待保存的关卡 | 先保存全部（Ctrl+Shift+S）再重启 |
| 开启插件后 PIE 变卡 | Pose Search / Insights 有常驻开销 | 开发期保留；做性能测试时临时关 `Gameplay Insights` |

---

## 6. 开启完成后你要告诉我的信息

开启并成功编译后，请回复一句「插件已开」即可。我会在实现轮做三件事：

1. 读取 `.uproject` 与编辑器写入结果对账，确认 11 个插件都在；
2. 落地 `Identity5.Build.cs` 与 `IDFCore` / `IDMMatch` 的 `Build.cs`（依赖清单见设计文档 2.2 节）；
3. 先写一个"能编译通过的最小 GAS 骨架"，用它验证插件确实可用（避免一次写太多再一起排查编译错误）。

> 若编译时报了与 `Build.cs` 无关的引擎级错误（例如某个插件在你的引擎安装里缺失），把错误贴给我，我按 C 组思路给替代方案。
