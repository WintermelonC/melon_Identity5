# 设计方案（第 1 轮）

> **项目代号**：Identity5（UE5 复刻《第五人格》核心玩法）
> **引擎版本**：Unreal Engine 5.8.3（`EngineAssociation: "5.8"`，已确认本机安装路径 `G:\Epic Games\UE_5.8`）
> **文档性质**：总体设计方案（不含具体代码）。第 1 轮只做「怎么搭、怎么拆、怎么跑通」的决策，代码从第 2 轮开始按里程碑落地。
> **数值声明**：本文中所有具体数值（冷却、时长、进度速率等）分为两类——
> `[还原]` 尽量贴近原作观感的目标值；`[待标定]` 需要实机对局验证后调整的初值。它们全部落在 DataAsset / DataTable 中，**不写死在 C++**。

---

## 0. 产品定位与范围

### 0.1 目标

做一个**可 5 人联网对局**的 4v1 非对称对抗核心循环复刻，重点不是美术还原度，而是：

1. 把原作的**核心循环**跑通：破译 → 追击 → 交互（板/窗）→ 击倒 → 挂椅 → 救援 → 开门 → 逃脱/淘汰；
2. 用**工业化 UE 技术栈**实现：GameplayAbilitySystem（GAS）+ Gameplay Feature 模块化 + 服务器权威网络同步 + CommonUI 流程；
3. 建立**可扩展的角色/地图数据管线**：加一个角色 = 加一个模块 + 若干数据资产，不改核心框架。

### 0.2 本轮（第 1 轮）交付内容

| 项 | 内容 | 状态 |
|---|---|---|
| 地图 | 军工厂（1 张，含厂房/集装箱/废墟等分区与出生点） | 本设计 |
| 交互点 | 密码机、大门电闸、狂欢之椅、木板、窗户、地窖、宝箱、信号枪拾取 | 本设计 |
| 角色 | 求生者 4：医生、幸运儿、律师、空军；监管者 2：红蝶、小丑 | 本设计 |
| 技术 | 网络同步（服务器权威 + 预测/回滚）、Gameplay Feature、GAS、UMG/CommonUI、AI（行为树/StateTree） | 本设计 |
| 明确不做 | 排位/赛季、外观抽卡、Persona 人格天赋完整树、全部 40+ 角色、美术最终资产 | 第 2 轮后评估 |

### 0.3 非目标（防止范围蔓延）

- 不做**手游触屏双摇杆**适配（PC 键鼠/手柄优先，触屏留接口不实现）；
- 不做**语音/文字聊天**（第 1 轮用固定快捷语补位，接口预留）；
- 不做**观战系统**（第 3 轮再评估，`APlayerState` 已预留 `bIsSpectator` 状态位）；
- 不使用 Lyra 的 ShooterCore 内容（仅借鉴其工程组织思路，不含任何 Lyra 资产与代码拷贝）。

### 0.4 关键假设与待确认项

| 编号 | 假设 | 影响 | 若假设不成立 |
|---|---|---|---|
| A1 | 对局规模固定 **4 求生 + 1 监管**，单局 1 张图，无中途加入 | 决定大厅/匹配/回填策略 | 需要加中途加入与角色替换逻辑 |
| A2 | 玩家人数 5 人全部由真人控制（监管者不接 AI 接管） | 决定是否需要 AI 补位 | 需要实现 Hunter AI 兜底 |
| A3 | **7 台密码机破译 5 台**开启大门（原作规则）`[还原]` | 决定胜负条件与节奏 | 需要抽象为 `MatchRuleSet` 数据资产，已在设计中抽象 |
| A4 | 单人开发/小队开发，物理资源有限，美术走"占位 → 替换"路线 | 决定并行度与里程碑长度 | 可压缩/拉长各 M 阶段 |
| A5 | 目标平台 Windows PC（DX12/SM6），UE5.8 默认渲染管线（Lumen + Nanite + VSM） | 决定性能预算 | 移动端需要切 SM5/前向渲染并重定预算 |

> ⚠️ **合规声明**：本项目为**学习/技术研究性质**的玩法复刻，不发布、不商用。《第五人格》相关名称、角色、地图、美术与音频素材版权归网易所有；工程内**不得**使用任何提取自原作的资产（模型/贴图/音频/数据表）。所有资产使用自研占位或商用授权素材，命名沿用原作仅为便于对照。

---

## 1. 技术选型

### 1.1 引擎与构建

| 项 | 选择 | 理由 |
|---|---|---|
| 引擎版本 | UE 5.8.3（已安装 `G:\Epic Games\UE_5.8`，另有 5.7 备份） | 项目 `EngineAssociation=5.8`，新特性（GameplayCameras、MotionTrajectory、Iris 稳定化）可用 |
| RHI / Shader Model | DX12 / SM6，Nanite + Lumen + VSM（当前 `DefaultEngine.ini` 已开启） | 复刻美术风格需要动态 GI 与大量模块化厂房资产 |
| 构建目标 | `Identity5`（Game）+ `Identity5Editor`（Editor），后续追加 `Identity5Server`（Dedicated Server） | 专用服务器用于自动化联调与压测 |
| 编译方式 | UBT + Live Coding；GA 类改动用 Live Coding，反射/接口改动重启编辑器 | 迭代速度 |
| 版本管理 | Git + Git LFS（`.uasset`/`.umap`/`.fbx`/`.wav` 走 LFS），忽略 `Binaries`/`Intermediate`/`DerivedDataCache`/`Saved` | 已有 `Identity5.sln`/`.slnx`，直接接入 |

### 1.2 插件启用清单（已逐项在本机 5.8 引擎目录中确认存在）

| 插件 | 用途 | 关键模块 |
|---|---|---|
| `GameplayAbilities` | GAS 主体（技能/属性/效果） | `GameplayAbilities`, `GameplayAbilitiesEditor` |
| `EnhancedInput` | 输入（已在 `Identity5.Build.cs` 依赖） | `EnhancedInput` |
| `GameplayCameras` | 相机系统（监管者视角/技能镜头/受击摇晃） | `GameplayCameras` |
| `CommonUI` | 跨平台 UI 框架与输入路由（大厅/对局 HUD/结算） | `CommonUI`, `CommonUIEditor` |
| `GameFeatures` | Gameplay Feature 插件装载 | `GameFeatures` |
| `ModularGameplay` | `UGameFrameworkComponentManager` / `IGameFrameworkInitStateInterface` 扩展点 | `ModularGameplay` |
| `GameplayInteractions` | 板/窗/救援等**交互状态机**（`UStateTree` 驱动，服务器权威） | `GameplayInteractions`, `GameplayInteractionsModule` |
| `GameplayStateTree` / `StateTree` | 交互点与 AI 的状态机数据资产 | `GameplayStateTree`, `StateTree` |
| `MotionWarping` | 翻窗/翻板/救援/攻击的位移对齐（避免穿模与滑步） | `MotionWarping` |
| `AnimationWarping` / `AnimationLocomotionLibrary` | 移动动画扭曲、步幅匹配（跑步/蹲行/受伤移位） | `AnimationWarping` |
| `PoseSearch` + `Chooser` | 姿势搜索选动画（追击急停急转）、参数选型 | `PoseSearch`, `Chooser` |
| `MotionTrajectory` | 运动轨迹预测（监管者冲刺/瞬移的目标与动画选择） | `MotionTrajectory` |
| `Niagara` | 破译进度、治疗、瞬移残影、恐惧震慑等特效 | `Niagara` |
| `Metasound` + `AudioModulation` + `SoundFields` + `SoundUtilities` | 心跳/追击音乐动态混音、空间音效 | `Metasound`, `AudioModulation` |
| `ReplicationGraph` / `Iris`（可选） | 5 人小规模暂不需要，作为压测阶段的网络优化备选 | `ReplicationGraph`, `Iris` |
| `GameplayInsights`（Editor） | GAS 调试：技能激活、属性变化时间线（开发期必备） | `GameplayInsights` |
| `DataValidation` | 数据资产校验（数值漏填/软引用断裂在 CI 前暴露） | `DataValidation` |
| `ControlRig` / `IKRig` | 角色绑定与 IK（脚部贴地、手部贴合交互点） | `ControlRig`, `IKRig` |
| `ModelingToolsEditorMode` | 已在项目启用：灰盒关卡建模 | — |

> **已排除**：`MassEntity` 全套（人群模拟，本作无大量 AI 单位需求，成本收益不匹配）。
> **注意**：UE5.8 中 `ModularGameplay` 插件**只有 `ModularGameplay` 一个模块**，不再提供 `ModularGameplayActors`（`AModularGameModeBase` 等类）；`GlobalDataLayer` 插件在 5.8 引擎目录中**未发现**。因此：**GameMode/GameState/PlayerController/Character 的扩展统一通过 `UGameFrameworkComponentManager` + `IGameFrameworkInitStateInterface` 实现**，不依赖上述两个已变动项。

### 1.3 核心架构决策（ADR 摘要）

| 编号 | 决策 | 备选方案 | 选择理由 |
|---|---|---|---|
| ADR-1 | **服务器权威 + 客户端预测** 的通用移动（`UCharacterMovementComponent`） | 纯服务器权威（无预测） | 翻窗/翻板/冲刺需要即时手感，纯权威延迟不可接受 |
| ADR-2 | **GAS 承载"角色能力 + 状态"，非 GAS 承载"交互点玩法"** | 全部塞进 GAS | 密码机/椅子/大门是多实体协作的**世界对象状态机**，用 ActorComponent + StateTree/复制状态更清晰，也避免滥用 GE |
| ADR-3 | **Gameplay Feature 插件按"角色/地图/模式/系统"切分** | 单一 Game 模块堆代码 | 满足需求 4 的模块化目标；后续可做"内容按需下载" |
| ADR-4 | **交互点统一抽象 `IInteractableTarget` + `FInteractionOption`** | 每个交互点各写一套 UI 提示 | 统一提示 UI（"长按 F 破译"）与输入路由，减少 4 个交互点 × UI 的重复 |
| ADR-5 | **所有时间/速率以服务器时间（`AGameStateBase::GetServerWorldTimeSeconds`）为准** | 客户端本地时间 | 破译进度、椅子计时、大门进度必须 5 端一致，避免作弊与漂移 |
| ADR-6 | **数值全部外置为 DataAsset/DataTable**，C++ 只写逻辑 | 蓝图里散落数值 | 便于批量调平衡、便于做"数值对照表"、便于单元测试 |
| ADR-7 | **进度类状态用"离散增量复制"（每 0.1s 或 1% 一次）而非每帧属性复制** | 每帧 `ReplicatedFloat` | GAS 属性复制有开销；破译进度不要求逐帧精度，用 0.5% 粒度即可，物理带宽降低约 20 倍 |
| ADR-8 | **GameplayMessageSubsystem 解耦系统通信**（自研轻量版，`UGameplayMessageSubsystem`） | 直接 Cast 引用各系统 | 破译完成/挂椅/开门等事件广播给 UI、音频、AI，避免系统间硬引用成网 |

---

## 2. 工程与源码结构

### 2.1 模块划分（插件 + 主模块）

```
Identity5.uproject
├── Source/
│   ├── Identity5/                     # 主模块：只放"启动与装配"，不放玩法
│   │   ├── Identity5.Build.cs
│   │   ├── Identity5GameMode.{h,cpp}          # AIdentity5GameMode（继承 AGameModeBase + 流程装配）
│   │   ├── Identity5GameInstance.{h,cpp}      # 登录/会话/本地玩家/UI 策略
│   │   ├── Identity5PlayerController.{h,cpp}  # 输入映射、UI 栈
│   │   ├── Identity5HUD.{h,cpp}               # 对局 HUD 壳（内部由 CommonUI 接管）
│   │   ├── Identity5WorldSettings.{h,cpp}     # 地图级默认技能/规则集
│   │   └── Identity5Globals.{h,cpp}           # 日志类别、GameplayTag 声明入口
│   └── Identity5Editor/
└── Plugins/GameFeatures/
    ├── IDFCore/            # ★ 核心框架：交互抽象、角色基类、GAS 胶水、消息、复制组件
    ├── IDMMatch/           # ★ 对局规则：MatchRuleSet、破译/大门/椅子/胜利条件、PlayerState
    ├── IDFMap_MilitaryFactory/  # 军工厂地图 + 该地图专属交互点摆放与区域逻辑
    ├── IDFSurvivor/        # 求生者公共：治疗、救援、翻窗翻板、道具栏、破译能力
    ├── IDFHunter/          # 监管者公共：攻击、恐惧半径、追击音乐、擦刀、绑椅
    ├── IDFChar_Doctor/     # 医生
    ├── IDFChar_LuckyGuy/   # 幸运儿
    ├── IDFChar_Lawyer/     # 律师
    ├── IDFChar_Airman/     # 空军
    ├── IDFChar_Geisha/     # 红蝶
    ├── IDFChar_Clown/      # 小丑
    └── IDFUI/              # 大厅/匹配/对局 HUD/结算（CommonUI）
```

**依赖方向（严格单向，禁止反向依赖）**：

```
IDFChar_*  ──▶ IDFSurvivor / IDFHunter ──▶ IDFCore ──▶ (Engine/GameplayAbilities)
IDMMatch   ──▶ IDFCore
IDFMap_*   ──▶ IDFCore + IDMMatch
IDFUI      ──▶ IDFCore + IDMMatch
Identity5(主模块) ──▶ 全部（仅做装配，不被任何 Feature 依赖）
```

> 📌 **勘误 / 补充（第 2 轮实施期确认）**：`IDFCore` 插件的**创建时机提前到 S05**（原计划 S10）。原因：GAS 骨架（自定义 ASC、AttributeSet、AbilitySet）就是本表所述的「GAS 胶水」，按本节归属 `IDFCore`；而 C++ 类**跨模块搬迁会产生资产断链**（S06 起 `DA_*`/`GE_*`/蓝图父类会引用这些类，跨模块移动后需要逐个人工重定向，并常留下 `REINST_` 孤儿类）。S05 时只有 4 个源文件、零资产引用，是零代价窗口。详见 `Documents/steps/S05-gas-skeleton.md` 第 -1 节。
>
> 连带影响：
> 1. **主模块 `Identity5.Build.cs` 需要依赖 `IDFCore`**（用 `PrivateDependencyModuleNames`），这符合上图的单向依赖（主模块 ──▶ 全部插件，无插件依赖主模块）；
> 2. 插件模块的导出宏是 **`IDFCORE_API`**，不是主模块的 `IDENTITY5_API`（写错会链接失败）；
> 3. **日志类别归属**：`LogIDFNet` / `LogIDFAbility` / `LogIDFAttribute` / `LogIDFInteraction` 随 GAS 骨架迁入 `IDFCore/Public/Logging/IDFCoreLog.h`，主模块只保留 `LogIdentity5`；`LogIDFMatch` 待 S10 建 `IDMMatch` 时迁入。原则是「平台类日志归主模块，系统类日志归所属插件」（与引擎自身一致：`LogGameplayAbilities` 定义在 GAS 插件内）。

### 2.2 GameFeature 插件内部标准结构

```
Plugins/GameFeatures/IDFChar_Doctor/
├── IDFChar_Doctor.uplugin            # Type=GameFeature, ExplicitlyLoaded=false
├── Content/
│   ├── Abilities/   GA_Doctor_SelfHeal.uasset ...
│   ├── Effects/     GE_Doctor_HealBoost.uasset ...
│   ├── Data/        DA_Doctor_Config.uasset（数值）
│   ├── Input/       IMC_Doctor.uasset, IA_Skill1.uasset
│   └── Characters/  BP_Doctor.uasset, ABP_Doctor.uasset
└── Source/IDFChar_Doctor/
    ├── IDFChar_Doctor.Build.cs
    ├── IDFChar_DoctorGameFeaturePolicy.{h,cpp}   # 注册输入/能力/UI 的注入点
    ├── DoctorCharacter.{h,cpp}                   # ADoctorCharacter : AIdentity5SurvivorCharacter
    ├── DoctorAbilitySet.{h,cpp}                  # UDataAsset 聚合：能力+属性+输入
    └── Abilities/GA_Doctor_*.{h,cpp}
```

### 2.3 命名与分层约定

| 前缀 | 含义 | 示例 |
|---|---|---|
| `A`/`U`/`F`/`E`/`I` | UE 标准 | `AIdentity5SurvivorCharacter` |
| `IDF` | 框架/系统层 | `IIDFInteractableTarget`, `UIDFInteractionComponent` |
| `IDM` | 对局规则层 | `AIDMGameState`, `UIDMMatchRuleSet` |
| `GA_`/`GE_`/`GC_`/`AT_`/`DA_` | 资产前缀 | `GA_Survivor_Vault`, `GE_Hunter_AttackCooldown` |
| `Ability.*` / `State.*` / `Event.*` | GameplayTag 根 | `Ability.Survivor.Decode`, `State.Survivor.OnChair.Phase1`, `Event.Machine.Decoded` |

---

## 3. 架构总览

### 3.1 分层图

```
┌──────────────────────────────────────────────────────────────────────┐
│ 表现层  CommonUI(大厅/匹配/HUD/结算) · GameplayCameras · Niagara · MetaSound │
├──────────────────────────────────────────────────────────────────────┤
│ 玩法层  GAS: AbilitySet/GameplayAbility/GameplayEffect/AttributeSet      │
│         GameplayInteractions + StateTree: 板/窗/救援/挂椅/破译 交互状态机   │
│         AI: BehaviorTree + StateTree + EQS（监管者追击/守椅/巡逻）          │
├──────────────────────────────────────────────────────────────────────┤
│ 规则层  IDMMatch: MatchRuleSet · 破译/大门/淘汰判定 · 胜负结算 · 逃脱/淘汰    │
├──────────────────────────────────────────────────────────────────────┤
│ 框架层  IDFCore: 交互抽象 · IDM GameMode/GameState/PlayerState/PlayerController│
│         GameFrameworkComponentManager(Extension) · GameplayMessage · 复制组件 │
├──────────────────────────────────────────────────────────────────────┤
│ 网络层  Actor Replication · RPC(Server/Client/NetMulticast) · 属性复制      │
│         预测: CharacterMovement / GAS PredictionKey / 自研 Interact 预测     │
└──────────────────────────────────────────────────────────────────────┘
```

### 3.2 类继承骨架

```
AGameModeBase ─────────────▶ AIDMGameMode
   └ 持有 UIDMMatchRuleSet(DataAsset)、对局状态机(EWarmup/Playing/PostMatch)

AGameStateBase ────────────▶ AIDMGameState
   ├ Replicated: 对局阶段、剩余密码机数、已破译数、大门状态、对局剩余时间
   └ 复制组件: UIDMMachineRegistryComponent / UIDMGateRegistryComponent / UIDMChairRegistryComponent

APlayerState ──────────────▶ AIDMPlayerState
   ├ Replicated: 阵营(ESurvivor/EHunter)、角色ID(GameplayTag)、道具、状态机快照(健康/椅子/淘汰)
   └ IAbilitySystemInterface: 每人一个 UIDMAbilitySystemComponent

APlayerController ─────────▶ AIDMPlayerController
   ├ 输入映射(EnhancedInput Mapping Context 按角色切换)
   ├ UI 栈(CommonUI UIDFPrimaryGameLayout)、观察者相机、断线重连占位

ACharacter ────────────────▶ AIDFCharacterBase (IAbilitySystemInterface + IGameFrameworkInitStateInterface)
                               ├ AIDFSurvivorCharacter ─▶ ADoctorCharacter / ALuckyGuyCharacter / ...
                               └ AIDFHunterCharacter  ─▶ AGeishaCharacter / AClownCharacter

AActor + IIDFInteractableTarget ─▶ AIDFInteractableActor
                               ├ AIDMMachineActor（密码机）
                               ├ AIDMGateActor（大门电闸）
                               ├ AIDMChairActor（狂欢之椅）
                               ├ AIDFPalletActor（板子）
                               ├ AIDFWindowActor（窗户）
                               ├ AIDMChestActor（宝箱）
                               ├ AIDMBasementActor（地窖）
                               └ AIDFPickupActor（信号枪等道具拾取）
```

---

## 4. 网络同步架构

### 4.1 拓扑与运行模式

| 模式 | 说明 | 用途 |
|---|---|---|
| `Listen Server`（PIE 1 Server + 4 Client） | 本机多进程联调 | 日常开发与回归 |
| `Dedicated Server` | `Identity5Server` Target + `-server -log` | 无头压测、自动化对局（AI 托管 4 个求生者） |
| `Standalone` | 单机 + 4 个 AI 补位 | 快速验证地图与交互点 |

**关键开关**（`DefaultEngine.ini`）：

```
[/Script/EngineSettings.GameMapsSettings]
GameDefaultMap=/Game/IDF/Maps/L_MainMenu
ServerDefaultMap=/Game/IDF/Maps/L_MilitaryFactory
GlobalDefaultGameMode=/Script/Identity5.Identity5GameMode

[SystemSettings]
net.UseAdaptiveNetUpdateFrequency=1
net.MaxNetUpdateFrequency=60
```

### 4.2 权威边界表（谁能改什么）

| 状态 | 权威方 | 客户端权限 | 复制方式 |
|---|---|---|---|
| 角色位置/朝向 | 服务器（CharacterMovement 内建预测） | 预测 + 校正 | 内建 |
| 血量/健康状态 | 服务器（GAS Attribute） | 只读预测（本地 UI 即时反馈） | `Replicated` 属性 + `RepNotify` |
| 破译进度 | 服务器 | 本地预测显示，每 0.1s 校准 | `ReplicatedFloat` + `OnRep` 平滑 |
| 板子状态（立/倒/被翻越 CD） | 服务器 | 无（仅表现预测） | `Replicated` + `RepNotify` |
| 椅子救援进度 / 椅子阶段 | 服务器 | 无 | 复制整型阶段 + 剩余时间 |
| 大门进度 / 开关状态 | 服务器 | 无 | 复制 + `RepNotify` |
| 技能冷却 | 服务器（GAS Cooldown GE） | 本地预测扣 CD，被拒绝则回滚 | GAS 内建 |
| UI 提示与交互可用性 | 客户端本地判定（只读快照） | 允许 | 不复制（由本地查询得出） |
| 匹配/选人/结算 | 服务器 | 按键上报 | RPC |

**铁律**：任何客户端可发起的动作（破译/翻板/救援/开锁/攻击/放技能）都走 `Server RPC` + **服务器端前置校验**（距离 / 朝向 / 状态 / GameplayTag 阻断 / 冷却），校验失败静默丢弃并记录 `UE_LOG(LogIDFNet, Verbose)`，不做客户端惩罚弹窗（避免作弊者探测规则）。

### 4.3 预测与回滚策略

| 交互 | 预测方案 |
|---|---|
| 翻窗 / 翻板 | 自定义 `UCharacterMovementComponent` 子类中实现 `FSavedMove_Character` 扩展（保存交互目标 + 阶段），配合 `MotionWarping` 对齐落点；服务器判定失败时通过 `ClientAdjustPosition` 回退 |
| 破译/治疗（持续类） | 客户端立即开始本地动画与进度预测，服务器确认后以服务器进度为准**平滑插值对齐**（0.25s 内收敛），不硬跳 |
| 监管者攻击 / 冲刺 | **不预测命中**：只本地预测动画与位移，伤害、击倒、擦刀由服务器裁定 |
| 道具投掷（信号枪） | 客户端预测生成投射物表现（本地特效），服务器生成权威投射物并广播结果；表现物与服务端不一致时以服务器为准重建 |
| 挂椅 / 救援 | 不预测，`Server RPC` → 服务器校验 → `NetMulticast` 播放表现 |

### 4.4 带宽预算（5 人房间 `[待标定]`）

| 项目 | 目标 |
|---|---|
| 单客户端下行 | ≤ 35 KB/s（峰值 ≤ 60 KB/s） |
| Actor 复制频率 | 角色 30 Hz、交互点 5~10 Hz（仅状态变化时）、投射物 30 Hz |
| 相关性与裁剪 | `NetCullDistanceSquared` 按对象设：角色 200m、交互点 150m、纯装饰 0（不复制，关卡内置） |
| RPC 频次上限 | 破译进度不上行（服务器自己算）；客户端仅上行"开始/停止破译"两次 |
| 属性复制 | 合并到 `OnRep`，每帧最多触发一次 UI 刷新；禁用每帧 `GetLifetimeReplicatedProps` 中无条件标记 |

### 4.5 断线/异常

| 场景 | 处理 |
|---|---|
| 求生者掉线 | `PlayerState.bDisconnected=true`，角色原地变 AI 托管（简单"跟随最近密码机"行为），180s 内可重连，超时按淘汰处理 `[待标定]` |
| 监管者掉线 | 对局暂停 30s 等待重连，超时判定求生者方胜利（`MatchRuleSet` 可配） |
| 服务器 Tick 抖动 | 破译/椅子计时基于 `GetServerWorldTimeSeconds` 累计，避免 `DeltaTime` 累加漂移 |
| 反作弊底线 | 服务器端全部动作做距离与状态校验；关键判定不信任客户端上报的任何值 |

---

## 5. 核心玩法系统（对局循环）

### 5.1 对局阶段状态机

```
[Lobby 大厅] ──选人/准备──▶ [Loading 加载] ──▶ [Warmup 出生保护 15s]
      ▲                                              │
      │                                              ▼
      │                                        [Playing 对局中]
      │                                              │
      │              ┌───────────────────────────────┼───────────────────────────────┐
      │              ▼                               ▼                               ▼
      │       [5 台密码机破译完成]             [4 人全部淘汰]                [大门打开并逃脱]
      │              ▼                               ▼                               ▼
      └──────────[PostMatch 结算] ◀──────────────────┴───────────────────────────────┘
```
- `Playing` 阶段内含子计时：椅子阶段倒计时、大门开启倒计时、地窖开启判定；
- 全流程由 `AIDMGameState::MatchPhase`（复制枚举）+ `AIDMGameMode` 的服务器状态机驱动，UI 只读。

### 5.2 胜负条件矩阵

| 结束条件 | 判定 | 备注 |
|---|---|---|
| 求生者胜利 | ≥1 人从大门或地窖逃脱 | 结算按逃脱人数分档 `[还原]`：3 逃 = 大胜 / 2 逃 = 胜 / 1 逃 = 平 |
| 监管者胜利 | 4 人全部淘汰（第 4 人被淘汰瞬间判定） | — |
| 时间耗尽 | 对局限时结束 `[待标定]`（自由练习模式可关） | 剩余逃生者视为逃脱 |
| 平局 | 逃生 1 人且时间耗尽 | 仅作为可选规则 |

### 5.3 核心循环时序（一次典型追击）

```
监管者发现 ─▶ 追击（心跳音乐起）
   ├─ 求生者绕板：板子立着 → 监管者破坏 / 求生者翻越
   ├─ 求生者翻窗：窗口交互 → 监管者翻窗（监管者翻窗速度更快 `[还原]`）
   ├─ 监管者命中：普通攻击 → 求生者进入"受伤/加速"状态（2s 加速 `[待标定]`）
   └─ 二次命中：求生者倒地（可被扛起）
倒地被扛 ─▶ 寻找最近狂欢之椅（监管者扛人时移速降低）
   └─ 挂椅：椅子三阶段倒计时开始
        Phase1 → 队友救援（读条 1s `[还原]`）→ 半血复活 + 二次挂椅加速计时
        Phase1 无人救 → Phase2（进度显示为"半血"）→ Phase3 淘汰
队友救援后 ─▶ 治疗/继续破译 ─▶ 5 台机完成 ─▶ 大门通电（2 扇门）─▶ 开门读条 100% ─▶ 逃离
```

### 5.4 系统清单与拥有者

| 系统 | 拥有者（模块） | 关键类 |
|---|---|---|
| 匹配/选人 | `IDMMatch` + `IDFUI` | `UIDMLobbySubsystem` |
| 对局规则 | `IDMMatch` | `AIDMGameMode`, `AIDMGameState`, `UIDMMatchRuleSet` |
| 破译系统 | `IDMMatch` | `AIDMMachineActor`, `UIDMDecodeComponent` |
| 大门系统 | `IDMMatch` | `AIDMGateActor`, `UIDMGateProgressComponent` |
| 椅子/淘汰 | `IDMMatch` | `AIDMChairActor`, `UIDMChairTimerComponent` |
| 板/窗 | `IDFMap_*` + `IDFHunter/Survivor` | `AIDFPalletActor`, `AIDFWindowActor` |
| 道具栏 | `IDFSurvivor` | `UIDFItemComponent`, `UIDFItemDataAsset` |
| 治疗/救援 | `IDFSurvivor` | `GA_Survivor_Heal`, `GA_Survivor_Rescue` |
| 攻击/擦刀 | `IDFHunter` | `GA_Hunter_Attack`, `GA_Hunter_WipeBlade` |
| 恐惧半径/心跳 | `IDFHunter` | `UIDFTerrorRadiusComponent` |
| 地窖/宝箱 | `IDMMatch` / `IDFSurvivor` | `AIDMBasementActor`, `AIDMChestActor` |
| 结算/数据上报 | `IDMMatch` + `IDFUI` | `UIDMScoreSubsystem` |
| AI（AI 补位/自定义房间） | `IDFCore` | `AIDFHunterAIController`, `BT_Hunter`（第 3 轮） |

---

## 6. 地图设计：军工厂（L_MilitaryFactory）

### 6.1 设计原则

1. **复刻目标**：还原军工厂的**分区拓扑与破译点分布**（中央厂房、外围集装箱/废墟、板窗点位与密码机分布），而非逐厘米复刻美术；
2. **可玩性优先**：每个区域必须同时具备「破译位」「转点位（板/窗）」「视线阻断」三要素；
3. **对称性克制**：非对称地图，但**出生点分布公平**——4 个求生者出生点环形分布，监管者出生点位于地图几何中心附近且与最近求生者距离 ≥ 35m `[待标定]`；
4. **面积预算**：约 130m × 130m 可玩区域（原作军工厂体感规模）`[待标定]`，外围留 20m 缓冲不可达装饰区。

### 6.2 分区表（灰盒布局）

| 编号 | 区域 | 内容 | 破译机 | 转点位 | 备注 |
|---|---|---|---|---|---|
| Z1 | 中央厂房（两层） | 钢架天车、楼梯、二层回廊、外部传送带 | 2 台（1F、2F） | 2 窗 1 板 | 视觉地标；二层有俯瞰视野 |
| Z2 | 集装箱堆场（东南） | 双排叠放集装箱、窄道 | 1 台 | 2 板 1 窗 | 强板区，狭窄易被堵 |
| Z3 | 废弃车辆/机库（西北） | 卡车残骸、机库棚架 | 1 台 | 1 板 1 窗 | 长通路，监管者冲刺有利 |
| Z4 | 木栅/废墟（西南） | 断墙、木堆、枯树 | 1 台 | 2 板 | 视野破碎，适合潜伏 |
| Z5 | 油罐区（东北） | 大型储罐、管道桥 | 1 台 | 1 板 1 窗 | 有地窖候选点 |
| Z6 | 大门 A（正南）/ 大门 B（正北偏东） | 电闸、开门交互点、通电后灯光 | — | — | 两扇门间距 ≥ 90m |
| Z7 | 地下室（Basement） | 4 张狂欢之椅 + 1 个宝箱 | — | — | 地窖不在此（原作地窖独立） |
| Z8 | 地窖点（3 处候选） | 随机 1 处开启 | — | — | 破译 ≥2 台后开启 `[还原]` |
| Z9 | 中场过渡带 | 断墙、木箱、板子 | 1 台（合计 7 台） | 2 板 1 窗 | 连接各区的"高速路" |

**密码机总计 7 台**（`[还原]` 原作 7 台 / 需破译 5 台），分布：Z1×2、Z2×1、Z3×1、Z4×1、Z5×1、Z9×1。

### 6.3 关卡组织

| 资产 | 路径 | 说明 |
|---|---|---|
| 主关卡 | `Content/IDF/Maps/L_MilitaryFactory` | 灰盒 → 美术替换（5 人图不需要流送；用 Level Instance 管理区域） |
| 子关卡/Level Instance | `LI_Z1_CentralFactory` … `LI_Z9_Midfield` | 分区独立编辑，便于多人分工 |
| 出生点 | `APlayerStart` × 5 + `AIDFSpawnPointSet`（Tag 标注 `Spawn.Survivor.A/B/C/D`、`Spawn.Hunter`） | 服务器按规则随机分配 |
| 导航 | `NavMeshBoundsVolume` 覆盖全部可玩区 + `NavModifierVolume` 排除板子倒地区域（倒地后更新） | AI 与寻路 |

### 6.4 交互点摆放规范（每个交互点 Actor 的必填属性）

| 属性 | 说明 |
|---|---|
| `InteractionTag` | `Interact.Machine` / `Interact.Pallet` … |
| `RegionTag` | `Region.Z2.Container`（用于 AI 战术与统计） |
| `ApproachPoints` | 一组 `ATargetPoint`（翻越/交互的贴近点，供 MotionWarping 与 AI 使用） |
| `ChaseValue` | 该点的"转点价值"评分（AI 与关卡评估用）`[待标定]` |
| `bShowOnLawyerMap` | 律师地图是否显示 |

---

## 7. 交互点系统设计

### 7.1 统一交互抽象（ADR-4）

```
IIDFInteractableTarget
 ├ GetInteractionOptions(Interactor) -> TArray<FIDFInteractionOption>
 │    { Tag, 显示文本, 需要长按?时长, 可用性(是否灰显+原因), 优先级, 目标Transform }
 ├ OnInteractionBegin/Update(DeltaTime)/End(Interactor, Option)
 └ CanInteract(Interactor, OutReason) -> bool

UIDFInteractionComponent（挂在角色上，服务器+客户端都有）
 ├ 每 0.1s 客户端扫描（球形 Trace，半径 2.0m，只查 Interaction 通道）→ 缓存在 UIDFInteractionPromptViewModel
 ├ 输入：IA_Interact(单击) / IA_InteractHold(长按) → 走 Server RPC(OptionTag, TargetActor, SequenceID)
 ├ 服务器二次校验：距离/朝向/状态/冷却/互斥 → 通过则 Begin
 └ 表现：CommonUI 交互提示条（含进度环）、圆形/长按图标、灰显原因
```

**互斥规则**：同一时刻只能有一个"占用型交互"（破译/救援/治疗/开箱/开门），通过 GameplayTag `State.Interacting.*` + 能力的 `BlockAbilitiesWithTag` 实现互斥；被监管者命中的瞬间，`Event.Combat.Hit` 广播强制 `CancelAbilities`。

### 7.2 七类交互点规格

#### 7.2.1 密码机（AIDMMachineActor）
| 项 | 规格 |
|---|---|
| 交互方式 | 长按/保持 F 持续破译；可多人同时破译（协作加速：第 2 人 +100% → 边际递减，第 3/4 人各 +50% `[待标定]`） |
| 进度模型 | `DecodeRate = Base * CharMultiplier * BuffMultiplier * (1 - 校准失败惩罚)`，服务器按 `DeltaTime` 累加 |
| 校准（QTE） | 破译过程中随机每 6~10s 出现一次校准圈，玩家需在指针进入判定区时点击；成功 +1% 加速，失败「爆机」→ 退进度 8% + 位置暴露给监管者 5s `[还原/待标定]` |
| 完成事件 | 广播 `Event.Machine.Decoded`（GameplayMessage）→ 全场计数 +1、监管者 HUD 提示、大门通电判定 |
| 复制 | `Progress(0..1)` 每 0.1s 或变化 >0.5% 时复制；`bDecoded` 为一次性 `RepNotify`；破译中的角色列表仅服务器持有（客户端只看到进度与特效） |
| 表现 | Niagara 火花、Metasound 机械声随进度加速、破译中角色手部 IK 贴合 |

#### 7.2.2 大门电闸（AIDMGateActor）
| 项 | 规格 |
|---|---|
| 开启条件 | `DecodedCount >= RuleSet.RequiredMachines`（默认 5）→ 广播 `Event.Gate.Powered` |
| 交互 | 长按 F 读条；进度 0→100%，读条期间松开进度**缓慢回退**（原作行为 `[还原]`，回落速率 5%/s `[待标定]`） |
| 开门结果 | `bOpened` → 门扇动画 + 灯光变化 + 音频提示；门开后持续保持开启 |
| 复制 | `Progress` 0.1s 粒度；`bPowered`/`bOpened` RepNotify |
| 逃脱判定 | 角色越过门内触发体（`ATriggerVolume`）→ 服务器标记 `bEscaped` → 角色隐藏+禁用碰撞（保留 PlayerState 供结算） |

#### 7.2.3 狂欢之椅（AIDMChairActor）
| 项 | 规格 |
|---|---|
| 状态机 | `Idle → Occupied(Phase1) → Phase2 → Escaped(淘汰)`；`Phase1 → Rescued(被救) → Idle(CD 3s)` |
| 计时 | Phase1 `30s`、Phase2 `25s`、Phase3 淘汰 `[待标定，贴近原作 30/25 观感]`；二次上椅从 Phase2 起算 `[还原]` |
| 挂椅 | 监管者扛人靠近 → `GA_Hunter_PutOnChair`（MotionWarping 对齐座位）→ 服务器把求生者 `AttachToComponent` 到椅子，禁用移动/能力（`State.OnChair`） |
| 救援 | 队友长按 F 读条 `1.0s` `[还原]`；成功后求生者恢复至"半血（受伤态）"、`State.Rescued`、获得短暂加速 2s `[待标定]` |
| 挣扎 | Phase2 期间求生者可主动挣扎（QTE 判定条），成功后小幅延长阶段计时；完整挣脱机制放第 3 轮 `[还原部分]` |
| 椅子位置 | 全场 12~16 张（含地下室 4 张），每张 `bIsBasement` 标记（地下室椅子 Phase1 时间更长 `[还原]`） |
| 空椅冷却 | 被救援后该椅进入 3s CD，期间不可再次挂人（防止无限连挂）`[待标定]` |

#### 7.2.4 木板（AIDFPalletActor）
| 项 | 规格 |
|---|---|
| 状态 | `Standing`（立起，可翻越/可被破坏）→ `Falling`（被推倒）→ `Broken`（被监管者破坏，永久消失） |
| 求生者翻板 | 靠近按压 → `GA_Survivor_PalletVault`：位移对齐 + 0.5s 动画；若板子立着且监管者在板对面，翻越后自动推倒（砸板：命中监管者则短眩晕 1.5s `[待标定]`） |
| 监管者破坏板 | 靠近长按 → `GA_Hunter_BreakPallet`：读条 1.2s `[待标定]`，破坏期间可被求生者砸板打断 |
| 复制 | `EPalletState` + `RepNotify` 播放翻倒动画；服务器权威 |
| AI 辅助 | `NavModifierVolume` 随状态动态启用（倒地/破坏后清出一条路） |

#### 7.2.5 窗户（AIDFWindowActor）
| 项 | 规格 |
|---|---|
| 交互 | 翻窗：`GA_Survivor_WindowVault` 0.6s / `GA_Hunter_WindowVault` 0.8s（监管者更快 `[待标定]`） |
| 冷却 | 单窗翻越 CD：求生者 1.2s / 监管者 2.0s `[待标定]`（防止来回刷窗无解） |
| 表现 | MotionWarping 对齐翻越路径 + 手部 IK 撑窗；翻越期间的受击免疫窗口需实机验证后确定 `[待标定]` |
| 复制 | 几乎无状态（仅复制翻越中的角色动作），服务器做 CD 校验 |

#### 7.2.6 宝箱（AIDMChestActor）
| 项 | 规格 |
|---|---|
| 交互 | 长按 4s `[待标定]`，随机开出一件道具（按 `RuleSet.ChestLootTable` 权重表） |
| 道具池 | 医疗包、手电、信号枪、工具箱、地图碎片 `[还原]`（第 1 轮实现前 3 种 + 空手） |
| 复制 | 仅服务器 Roll，结果通过 `ClientRPC` 给开箱者 + 表现广播 |
| 消耗 | 每箱一次性；被开过显示空箱状态 |

#### 7.2.7 地窖（AIDMBasementActor）
| 项 | 规格 |
|---|---|
| 开启条件 | 场上仅剩 1 名求生者且 `DecodedCount >= 2` `[还原]` |
| 交互 | 跳入 → 立即判定逃脱（`Event.Survivor.Escaped`） |
| 位置 | 3 处候选点随机 1 处激活，未激活的点不显示 |
| 复制 | `bActive` RepNotify（开启时给全场音效提示） |

### 7.3 交互点状态机统一实现（GameplayInteractions + StateTree）

每个交互点 Actor 挂 `UGameplayInteractionUserComponent`（或自研轻量 `UIDFInteractableStateComponent`），状态机资产化：

```
StateTree_ST_Machine:
  Root(Idle)
   ├ Idle ──[Event.InteractBegin && CanInteract]──▶ Decoding(Enter: 注册占位角色)
   ├ Decoding ──[Tick: Progress += Rate*Dt]──▶ Decoding (自转移，UI 更新)
   ├ Decoding ──[Event.InteractEnd || Event.Combat.Hit || Progress>=1]──▶ Idle
   └ Idle ──[Progress>=1]──▶ Decoded(Enter: 广播 Event.Machine.Decoded, 禁用交互)
```

**选择 StateTree/GameplayInteractions 而非纯 GAS 的收益**：交互点是"世界对象"，其状态树可用编辑器可视化调试、可被设计师改条件，而不必为每个交互点写一套 GameplayAbility；同时 GAS 只处理**角色侧**的能力（谁在读条、读到哪、被打断怎么恢复），两侧通过 `Event.*` GameplayTag + GameplayMessage 通信。

### 7.4 道具系统（求生者道具栏）

| 项 | 设计 |
|---|---|
| 数据结构 | `UIDFItemDataAsset`（图标、名称、能力类、堆叠、使用时间、耐久） |
| 运行时 | `UIDFItemComponent`（挂在求生者身上）：`CurrentItemId` + `Uses`，复制到所有客户端（HUD 显示） |
| 使用 | 道具即"授予一个临时 GameplayAbility"（`Ability.Item.*`），使用完毕移除 |
| 第 1 轮道具 | 医疗包（自疗 1 次，比医生慢 40% `[待标定]`）、手电筒（照射监管者致盲，需正对 1.5s）、信号枪（通用拾取版：命中眩晕 3s `[待标定]`） |
| 掉落 | 被击倒时掉落道具，队友可拾取 `[还原]` |

---

## 8. 角色设计

### 8.1 角色通用规格

**求生者（Survivor）公共**：

| 项 | 值 |
|---|---|
| 移动速度 | 4.0 m/s（跑）/ 2.0 m/s（蹲）`[待标定]` |
| 生命状态 | `Healthy → Injured(受伤, 可治疗) → Downed(倒地, 可被扛) → OnChair → Escaped/Eliminated` |
| 受击加速 | 被命中后 2.0s 内 +50% 移速 `[还原观感]` |
| 治疗 | 自疗基础 12s；他人治疗 8s；医生自疗 8s `[待标定]` |
| 翻窗/CD | 见 7.2.5 |
| 交互速度基准 | 破译 1.0（100% 基线，即 1 台机约 75s `[待标定]`；原作单机 80s 左右） |
| 视野 | 第三人称越肩相机（GameplayCameras），FOV 90 |
| 能力互斥 | `State.Downed / State.OnChair / State.Vaulting` 时禁用大部分能力 |

**监管者（Hunter）公共**：

| 项 | 值 |
|---|---|
| 移动速度 | 4.6 m/s（略快于求生者，靠板窗与加速件拉开）`[待标定]` |
| 攻击 | 普通攻击（前方扇形 2.5m），命中进入擦刀（Wipe Blade）1.5s 后摇 `[待标定]` |
| 攻击 CD | 命中后 2.5s / 空刀 1.0s `[待标定]` |
| 恐惧半径 | 默认 24m，进入半径播放心跳与追击音乐 `[待标定]` |
| 扛人 | 扛起倒地求生者，移速降至 3.6 m/s，可被板子砸落 `[待标定]` |
| 视野 | 第一人称（原作监管者为第一人称），相机挂点 + 手臂动画（FPP 动画与 TPP 分离） |
| 胜利反馈 | 每次命中/击倒触发 GameplayCue（屏幕反馈、音效、震动） |

### 8.2 求生者角色卡

#### 8.2.1 医生（Doctor）— `IDFChar_Doctor`
| 项 | 设计 |
|---|---|
| 定位 | 续航/治疗核心，团队持续作战能力 |
| 被动「医术」 | 治疗速度 +30%；自疗无需医疗包（可用自身技能治疗）`[还原]` |
| 主动技能「医术精研」 | 自我治疗（读条 8s），第 1 轮为静止读条；CD 60s `[待标定]` |
| 受伤加速 | 医生在被击中后治疗速度短暂提升 `[第 3 轮]` |
| GAS 实现 | `GA_Doctor_SelfHeal`（`Ability.Survivor.Skill.Doctor`）+ `GE_Doctor_HealBoost`（治疗速率乘区）+ `GE_Doctor_Vitality`（受击后治疗加速，第 3 轮） |
| 数据 | `DA_Doctor_Config`：`HealRateMultiplier=1.3`, `SelfHealDuration=8.0`, `SelfHealCooldown=60` |
| 独占输入 | `IA_Skill1`（自我治疗） |
| 特色网络点 | 治疗进度需施治者/受治者双方预测显示，服务器裁定 |

#### 8.2.2 幸运儿（Lucky Guy）— `IDFChar_LuckyGuy`
| 项 | 设计 |
|---|---|
| 定位 | 万金油/道具流，无固定技能但成长性强 |
| 被动「好运」 | 开箱速度 +50%；开箱**必定**获得道具（不会开出空箱）`[还原]` |
| 主动技能「幸运祈愿」 | 打开"许愿"界面，从 3 个随机道具中选 1 个获得（每局 1 次）`[还原]` |
| GAS 实现 | `GA_LuckyGuy_Wish`（`Ability.Survivor.Item.Wish`）；开箱加速用 `GE_LuckyGuy_ChestSpeed`（修改 `ChestSpeed` 乘区） |
| 数据 | `DA_LuckyGuy_Config`：`ChestSpeedMultiplier=1.5`, `WishCharges=1`, `WishOptionCount=3` |
| 特色网络点 | 许愿结果**必须服务器 Roll**（防作弊），结果通过 `ClientRPC` 下发选项，玩家选择再上行确认 |

#### 8.2.3 律师（Lawyer）— `IDFChar_Lawyer`
| 项 | 设计 |
|---|---|
| 定位 | 信息位/破译辅助 |
| 被动「地图」 | 开局持有地图（HUD 小地图显示密码机、大门、队友位置；监管者位置每 20s 短暂显示）`[还原]` |
| 主动技能「绘制地图」 | 标记一台密码机，使**全队**对其破译速度 +20%，持续 30s；CD 60s `[待标定]` |
| 破译协作 | 律师附近 8m 内的队友破译速度 +10%（鼓舞）`[待标定]` |
| GAS 实现 | `GA_Lawyer_MarkMachine`（`Ability.Survivor.Skill.Lawyer`）→ 服务器在目标机对破译者施加 `GE_Machine_MarkedDecodeBoost`；地图数据由 `IDMMachineRegistryComponent` 快照下发给律师客户端 |
| 数据 | `DA_Lawyer_Config`：`MarkDecodeBonus=1.2`, `MarkDuration=30`, `MarkCooldown=60`, `MapRevealInterval=20` |
| 特色网络点 | 信息类技能的"可见性"必须服务器过滤：**只有律师**收到监管者位置（`AIDMPlayerController::ClientReceiveHunterIntel`） |

#### 8.2.4 空军（Airman）— `IDFChar_Airman`
| 项 | 设计 |
|---|---|
| 定位 | 硬控/救援位，一枪改变团战 |
| 初始道具「信号枪」 | 开局携带 1 发，命中监管者 → 眩晕 4s `[还原/待标定]`；未命中则浪费 |
| 被动「纪律」 | 附近 8m 内监管者的攻击后摇延长 30% `[待标定]` |
| 救援增益 | 空军救援椅子上的队友时，救援读条 -30% `[待标定]` |
| GAS 实现 | `GA_Airman_FlareGun`（`Ability.Survivor.Item.FlareGun`）：服务器生成 `AIDFProjectile_Flare`；命中 → `GE_Hunter_Stunned`（禁用移动/能力 4s + 白闪 Cue） |
| 数据 | `DA_Airman_Config`：`FlareStunDuration=4.0`, `FlareCooldown=60`, `RescueSpeedMultiplier=1.3`, `HunterRecoveryDebuff=0.3` |
| 特色网络点 | 投射物服务器权威 + 客户端预测表现；眩晕用 `State.Hunter.Stunned` 标签驱动动画与输入禁用 |

### 8.3 监管者角色卡

#### 8.3.1 红蝶（Geisha）— `IDFChar_Geisha`
| 项 | 设计 |
|---|---|
| 定位 | 高机动瞬移型，无视距离的压迫感 |
| 被动「美人相」 | 恐惧半径表现与原作"视线标记"机制，第 1 轮简化为常态心跳 `[还原部分]` |
| 主动技能「刹那生灭」（冲刺） | 朝正前方瞬移 8m，需视线通畅（不穿墙）`[待标定]`；CD 10s |
| 主动技能「燕返」（标记瞬移） | 对 30m 内**有视线**的求生者施加"蝶印"（仅红蝶可见），再次施放可瞬移至其身后（期间不可攻击），CD 20s `[还原观感]` |
| 相态切换「三相」 | `美人相`（默认，移速较快）↔ `般若相`（动作/交互更快，移速略降）；第 1 轮实现基础切换 + 一项加成 `[还原部分]` |
| GAS 实现 | `GA_Geisha_Phase`（相态切换，修改移速/动作速度乘区）、`GA_Geisha_Dash`（RootMotion + MotionWarping）、`GA_Geisha_Mark`（施加 `State.Hunter.Mark.Target`）、`GA_Geisha_Blink`（瞬移 + 相机过渡 + Niagara 残影） |
| 数据 | `DA_Geisha_Config`：`DashDistance=8.0`, `DashCooldown=10`, `BlinkRange=30`, `BlinkCooldown=20`, `BeautySpeedMul=1.0`, `HannyaSpeedMul=0.9`, `HannyaActionSpeedMul=1.15` |
| 特色网络点 | 瞬移类技能**必须服务器裁定起点终点与碰撞**；标记的可见性需按客户端过滤（覆写 `IsNetRelevantFor` 或只发给红蝶） |

#### 8.3.2 小丑（Clown）— `IDFChar_Clown`
| 项 | 设计 |
|---|---|
| 定位 | 直线冲刺/资源管理型（火箭燃料），滚雪球 |
| 主动技能「火箭冲刺」 | 消耗 1 格燃料向前高速冲刺（18 m/s，持续至撞墙/撞板/时间到），冲刺中撞击求生者造成 1 次伤害；冲刺中可低速率转向 `[还原/待标定]` |
| 燃料资源 | 上限 3 格，随时间自动回复（每格 20s）+ 拾取"推进器零件"（地图上 3 处刷新的可拾取物）立即 +1 `[还原]` |
| 撞墙惩罚 | 冲刺撞墙 → 短眩晕 1.5s + 消耗掉本次燃料 `[待标定]` |
| GAS 实现 | `GA_Clown_Dash`（`Ability.Hunter.Skill.Clown.Dash`）：服务器启动 `UCharacterMovementComponent` 自定义 Dash 模式（走 `FSavedMove` 保证预测），碰撞用 Capsule Sweep 逐帧判定；燃料为 GAS Attribute（`RocketFuel`，`Replicated` + `PreAttributeChange` 做 Clamp），消耗/回复走 `GE_Clown_FuelCost/FuelRegen` |
| 数据 | `DA_Clown_Config`：`DashSpeed=18.0`, `MaxFuel=3`, `FuelRegenTime=20.0`, `WallHitStun=1.5`, `PartPickupCount=3` |
| 特色网络点 | 冲刺是**高频位移**，必须用 CharacterMovement 的 custom mode 而非 `SetActorLocation`，才能获得内建预测与校正；命中判定只在服务器做（避免客户端"拉角度"作弊） |

### 8.4 角色选择与装配流程

```
[匹配成功] → 服务器分配 5 个 PlayerState → [选人阶段 30s]
   ├ 求生者 4 人各选一名角色（不可重复）
   ├ 监管者 1 人选择
   └ 服务器 Load GameFeature 插件（IDFChar_Doctor 等）→ 生成对应 Character 类 + 注入 AbilitySet/InputMapping
[加载地图] → GameMode 读取 UIDMMatchRuleSet → 生成 7 机 / 2 门 / N 椅 / 板窗（地图内预摆放，服务器统一开关）
```

**装配点（Extension Point）**：用 `UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver` + `IGameFrameworkInitStateInterface`（`InitState.Spawned / DataAvailable / DataInitialized / GameplayReady`）在角色生成后异步注入：AbilitySet、InputConfig、UI Widget、相机配置、初始 GE。这样"角色 Feature 插件"不需要改主 GameMode 代码。

---

## 9. GAS 系统设计

### 9.1 ASC 选择与创建策略

| 对象 | ASC 位置 | Replication Mode | 理由 |
|---|---|---|---|
| 求生者/监管者 | `AIDFCharacterBase`（`IAbilitySystemInterface`） | `Mixed` | 自己受控角色全量预测，其他人只收 GE 结果 |
| `APlayerState` | 可选挂在 PlayerState（Lyra 风格，支持跨重生保留） | `Mixed` | 第 1 轮**先挂 Character**（无重生需求），接口预留 `GetAbilitySystemComponent()` 查询 PlayerState |
| 交互点（密码机/椅子/大门） | **不创建 ASC** | — | 见 ADR-2；其状态用普通复制属性 |

**初始化顺序（关键，写错就崩或竞态）**：
`PossessedBy(server)` → `InitAbilityActorInfo(Owner, Avatar)` → 应用 `UIDFAbilitySet`（授予能力 + 初始 GE + 属性）→ `InitState.DataInitialized` → `GameplayReady`（此时才允许技能激活与 UI 订阅）。

### 9.2 AttributeSet 划分

| AttributeSet | 归属 | 属性 |
|---|---|---|
| `UIDFHealthSet` | 所有角色 | `Health`(100)、`MaxHealth`、`Damage`(Meta)、`Healing`(Meta) |
| `UIDFSurvivorSet` | 求生者 | `DecodeSpeed`(1.0)、`HealSpeed`(1.0)、`RescueSpeed`(1.0)、`VaultSpeed`(1.0)、`MoveSpeedMultiplier`(1.0)、`ChestSpeed`(1.0) |
| `UIDFHunterSet` | 监管者 | `AttackDamage`、`AttackRecoverySpeed`、`WipeBladeDuration`、`TerrorRadius`、`RocketFuel`(小丑)、`DashSpeed`(小丑)、`BlinkRange`(红蝶) |
| `UIDFMatchSet` | 挂 GameState（可选） | `DecodedMachineCount`、`EscapedSurvivorCount`、`EliminatedSurvivorCount`（供 UI 直接绑定） |

**Meta Attribute 约定**：伤害/治疗走 `Damage`/`Healing` 元属性 + `PostGameplayEffectExecute` 统一结算（先 Clamp 再广播 `Event.Combat.Damaged`），便于集中处理无敌帧、翻越免疫、擦刀期间免疫等规则。

### 9.3 GameplayTag 体系（第 1 轮必备集合）

```
Ability.Survivor.Decode / Heal.Self / Heal.Other / Rescue / Vault.Window / Vault.Pallet
Ability.Survivor.Item.MedicalKit / Flashlight / FlareGun / Wish
Ability.Survivor.Skill.Doctor / LuckyGuy / Lawyer / Airman
Ability.Hunter.Attack.Normal / WipeBlade / Carry / PutOnChair / BreakPallet / Vault.Window
Ability.Hunter.Skill.Geisha.Dash / Geisha.Mark / Geisha.Blink / Geisha.Phase
Ability.Hunter.Skill.Clown.Dash / Clown.PickupPart
State.Survivor.Healthy / Injured / Downed / Carried / OnChair.Phase1/2 / Escaped / Eliminated / Healing / Decoding
State.Hunter.Idle / Chasing / Attacking / WipingBlade / Stunned / Dashing / Carrying
State.Debuff.Exposed / Slowed / Blinded / Trapped
State.Interacting.Decode / Rescue / Heal / OpenGate / Chest
State.Match.Warmup / Playing / PostMatch
Event.Combat.Hit / Event.Machine.Decoded / Event.Gate.Powered / Event.Gate.Opened
Event.Survivor.Downed / Carried / OnChair / Rescued / Escaped / Eliminated
Event.Match.PhaseChanged / Event.Item.PickedUp / Event.Calibration.Success / Event.Calibration.Failed
Block.Ability.Movement / Block.Ability.Interact / Block.Ability.Attack
```

**标签驱动规则示例**：攻击能力 `ActivationBlockedTags = State.Survivor.Downed, State.Survivor.OnChair, State.Hunter.Stunned, State.Hunter.WipingBlade`；翻越能力 `ActivationOwnedTags = State.Vaulting` 且 `ActivationBlockedTags = State.Survivor.Carried`。

### 9.4 GameplayEffect 分类（命名即用途）

| 类别 | 示例 | 关键配置 |
|---|---|---|
| 属性初始化 | `GE_Init_BaseAttributes` | `Instant`，`SetByCaller` 或 DataAsset 驱动 |
| 持续状态 | `GE_State_Injured` | `Infinite` + `GrantedTags`，被移除时切回 Healthy |
| 瞬时伤害 | `GE_Damage_NormalAttack` | `Instant` + `SetByCaller(Damage)` + `GameplayCue.Hit.Flesh` |
| 位移/速度 | `GE_Buff_HitSpeedBoost`(2s, +50%)、`GE_Debuff_Slow` | `Duration` + `MoveSpeedMultiplier` 乘区（`MultiplyCompound`） |
| 冷却 | `GE_Cooldown_*` | `Duration` + `GrantedTags = Cooldown.Skill.X`，由 `UGameplayAbility::CooldownGameplayEffectClass` 引用 |
| 成本 | `GE_Cost_Clown_Fuel` | `Instant`，负向 `RocketFuel`，不足则能力不可激活 |
| 技能增益 | `GE_Machine_MarkedDecodeBoost`（律师）、`GE_Doctor_HealBoost` | `Duration` + `DecodeSpeed/HealSpeed` 乘区，`StackingType=AggregateBySource` |

**GameplayCue 规划**：`GameplayCue.Combat.Hit.*`、`GameplayCue.Skill.Geisha.Blink`、`GameplayCue.Skill.Clown.DashTrail`、`GameplayCue.Machine.CalibrationFail`、`GameplayCue.Match.GatePowered`；持续型特效走 `GameplayCueNotify_Actor`，一次性走 `_Static`；音频走 MetaSound + AudioModulation 参数（心跳强度 = 与监管者距离）。

### 9.5 能力规格模板（每个 GA 必填）

| 字段 | 说明 |
|---|---|
| `AbilityTags` / `ActivationOwnedTags` / `ActivationBlockedTags` / `CancelAbilitiesWithTag` / `BlockAbilitiesWithTag` | 标签契约（互斥与打断的核心） |
| `NetExecutionPolicy` | 主动技能 `LocalPredicted`；纯服务器裁定（挂椅/瞬移落点/道具 Roll）用 `ServerOnly`；被动用 `ServerInitiated` |
| `InstancingPolicy` | 一般 `InstancedPerActor`；瞬发能力可 `NonInstanced` |
| `CostGameplayEffectClass` / `CooldownGameplayEffectClass` | 成本与冷却 |
| `SourceBlockedTags` / `TargetBlockedTags` | 目标侧校验 |
| 预测容忍度 | `LocalPredicted` 技能必须实现 `OnTimedOut`/`CancelAbility` 的客户端回滚表现（UI 进度回退、特效停止） |

**必须实现的 GAS 胶水（IDFCore 提供）**：
1. `UIDFAbilitySystemComponent : UAbilitySystemComponent`：实现 `AbilityInputTagPressed/Released`、`OnAbilityInputTagHeld`、`TryActivateAbilityByTag`、`NotifyAbilityFailed`（统一失败反馈：提示 + 音效）、GE 应用日志（受 `LogIDFAbility` 控制）；
2. `UIDFAbilitySet : UPrimaryDataAsset`：把 `GrantedAbilities` / `GrantedEffects` / `AttributeSets` / `InputTag→Ability` 聚合为一份资产，`GiveToAbilitySystem()` 一键注入；
3. `UIDFInputConfig : UDataAsset`：`NativeInputActions` + `AbilityInputActions`（带 InputTag）绑定；
4. 能力失败/超时/标签变更的统一 UI 与音频反馈。

---

## 10. AI 设计（监管者 AI 与 AI 补位）

> 第 1 轮真人 5 人即可跑通，但**开发期必须能单机测试**，因此 AI 列入本轮（可降级为最小实现）。

| 项 | 设计 |
|---|---|
| 框架 | `AAIController` + `BehaviorTree` + `Blackboard` + `EQS` + `StateTree`（高层阶段用 StateTree，具体行为用 BT） |
| 感知 | `AIPerceptionComponent`：视觉（`AISense_Sight`，视锥 + 遮挡）+ 听觉（破译声/爆机声） |
| 黑板键 | `TargetSurvivor`、`LastKnownLocation`、`ChaseState`、`NearestChair`、`NearestMachine`、`bCarrying` |
| 高层状态 | `Patrol`（巡机）→ `Chase`（追击，含板窗决策）→ `Attack` → `Pickup/Carry` → `HangOnChair` → `CampChair`（守椅）→ `Return` |
| 板窗决策 | EQS 查询"求生者与最近板/窗的相对位置"→ 选择绕板 / 破板 / 翻窗；`ChaseValue` 数据驱动 |
| 求生者 AI（补位用） | StateTree 最小实现：`找最近未完成密码机 → 破译（校准简化）→ 监管者进入 12m 内 → 逃跑（沿预设逃脱路径点）`；不做救援与治疗 |
| 寻路 | `NavMesh` + `AIController::MoveTo`；板子倒地/破坏时 `NavModifierVolume` 开关触发局部导航重建 |
| 性能 | AI 更新频率：感知 0.5s、BT Tick 0.2s；单帧 AI 预算 ≤ 1ms |

---

## 11. UI / UX 设计（CommonUI）

### 11.1 层级与页面

```
UCommonGameViewportClient
└ UIDFPrimaryGameLayout（CommonUI 的 GameLayout 模式：Game / Menu / Modal 三层栈）
   ├ [Game Layer]  WBP_HUD_Match         对局 HUD（常驻）
   │     ├ 破译进度条 / 校准 QTE 圈 / 交互提示（长按环）
   │     ├ 求生者：队友状态条(4)、道具栏、技能按钮+CD、健康状态
   │     ├ 监管者：技能按钮+CD（燃料/相态）、追击提示、命中反馈
   │     └ 顶部：剩余密码机计数、对局计时、大门状态
   ├ [Menu Layer]  WBP_Lobby / WBP_Matchmaking / WBP_CharacterSelect / WBP_Settings / WBP_Scoreboard
   └ [Modal Layer] WBP_Result（结算：逃脱/淘汰、得分、数据统计）、WBP_Disconnected
```

### 11.2 关键 UI 组件

| 组件 | 说明 |
|---|---|
| `WBP_InteractionPrompt` | 由 `UIDFInteractionComponent` 的 ViewModel 驱动：图标 + 文本 + 长按进度环 + 灰显原因 |
| `WBP_CalibrationQTE` | 校准圈：指针旋转、判定区高亮；输入成功/失败播不同 GameplayCue；**判定在服务器**，客户端只做表现 |
| `WBP_ChairTimer` | 椅子阶段倒计时（自己/队友），Phase2 强化紧迫感 |
| `WBP_SkillButton` | 由 GAS 的 `FGameplayAbilitySpec` 冷却信息驱动（`GetCooldownTimeRemaining`），支持不可用灰显与原因 |
| `WBP_LawyerMap` | 律师专属小地图（仅该玩家收到数据） |
| `WBP_PlayerStatusBar` | 4 名求生者健康/椅子状态（服务器复制快照驱动） |

### 11.3 UI 数据流（ViewModel 模式）

```
服务器复制状态 ──▶ PlayerController/PlayerState 的 RepNotify ──▶ UIDFHUDViewModel（UObject）
   ──▶ MVVM View（UCommonUserWidget 子类，FieldNotify 绑定）──▶ 控件属性
输入：控件 → ViewModel → PlayerController → Server RPC（禁止 Widget 直接调 GAS）
```

**原则**：UI 层**绝不**直接修改游戏状态；所有写操作经 `PlayerController` 的 RPC。UI 只读 ViewModel，避免 UI 成为第二个权威源。

---

## 12. 数据驱动与配置资产

| 资产 | 内容 | 消费者 |
|---|---|---|
| `UIDMMatchRuleSet`(DA) | 机器总数 7 / 需破译 5、大门数 2、开门时间、椅子阶段时长、地窖条件、逃脱分档、时限 | `AIDMGameMode` |
| `DA_Character_*` | 各角色数值（速度/冷却/时长/倍率） | 各角色 AbilitySet |
| `UIDFAbilitySet`(DA) | 能力 + 初始 GE + 属性集 + 输入映射 | 角色装配 |
| `UIDFItemDataAsset` / `DA_ChestLootTable`(DT) | 道具与宝箱掉落权重 | 道具系统 |
| `DT_CharacterStats` | 全角色数值总表（横向对比与一键平衡） | 编辑器校验 + 导入 DA |
| `DT_MatchTuning` | 全局手感参数（受击加速、翻窗 CD、爆机惩罚…） | 全局系统 |
| `StateTree_ST_*` | 交互点/角色/AI 状态机 | GameplayInteractions / AI |
| `DA_MapLayout_MilitaryFactory` | 分区、机位、椅位、地窖候选（编辑器可视化） | 关卡校验工具 |

**校验（DataValidation 插件）**：所有 DA/DT 实现 `IsDataValid()`，检查：必填数值 > 0、软引用可解析、GameplayTag 存在于 `DefaultGameplayTags.ini`、`AbilitySet` 引用的 GA 具有冷却 GE。

---

## 13. 性能预算与优化

| 项 | 目标（1080p / 中高端 GPU `[待标定]`） |
|---|---|
| 帧率 | 1080p 60fps（最低 45fps），CPU Game Thread ≤ 8ms |
| 服务器 Tick | ≤ 30Hz，单帧服务器逻辑 ≤ 6ms |
| Draw Call | ≤ 2500 视口内（Nanite + 实例化 + HISM 处理集装箱/废墟重复件） |
| 内存 | ≤ 6 GB 常驻 |
| 角色骨骼 | ≤ 150 骨骼/角色，5 个角色均开 `URO`（Update Rate Optimization） |
| 特效 | Niagara 同屏 ≤ 30 个活跃系统；破译/治疗特效按需 `Activate` |
| 网络 | 见 4.4 |
| 音频 | 心跳/音乐用 AudioModulation 单参数控制，不叠加多套音乐资源 |

**关键优化手段**：
1. 集装箱/木箱/废墟用 `HISM` + `Nanite`，减少 Actor 数量；
2. 交互点 Actor 只在状态变化时复制（`SetNetUpdateFrequency(5)` + 按需 `ForceNetUpdate`）；
3. 角色动画用 `PoseSearch` + `Chooser` 代替大量 AnimBP 分支，降低 Anim 线程开销；
4. GAS 效果查询用 `FGameplayTagContainer` 缓存，避免每帧重复匹配；
5. `GameplayInsights` + `Unreal Insights` + `stat net` 作为常规回归工具。

---

## 14. 测试与验证方案

### 14.1 分层测试

| 层 | 手段 | 覆盖 |
|---|---|---|
| 单元/自动化 | UE Automation Spec（`IMPLEMENT_SIMPLE_AUTOMATION_TEST`） | MatchRuleSet 胜负判定、进度累加与爆机惩罚、椅子阶段推进、地窖开启条件、GameplayTag 契约完整性 |
| 集成 | `FunctionalTest` 关卡 + Gauntlet 自动化对局 | 5 人（1 真人 + 4 AI）跑完整局；破译→开门→逃脱全链路 |
| 网络 | PIE 多客户端 + `net.PktLag=150 / PktLoss=3` 模拟 | 预测校正不穿模不回弹；破译进度 5 端一致（误差 < 1%） |
| 压力 | Dedicated Server + 并发房间 | 20 房间 × 5 人，服务器 CPU 与带宽曲线 |
| 手工 | 检查单（Checklist） | 每个交互点在 Standalone/Client/Server 三种模式下的行为一致性 |

### 14.2 验收标准（本轮 DoD）

| 编号 | 验收项 | 通过条件 |
|---|---|---|
| AC-1 | 地图可玩 | 军工厂灰盒完成，9 个分区连通，导航网格无断裂 |
| AC-2 | 交互点 | 7 机 / 2 门 / ≥12 椅 / ≥14 板 / ≥8 窗 / 1 地窖 / ≥2 宝箱 全部可交互且状态正确同步 |
| AC-3 | 全流程 | 5 人（1 真人 + 4 AI）能在 Listen Server 下完成一局：破译 5 台 → 开门 → 逃脱；以及 4 人全淘汰分支 |
| AC-4 | 角色 | 4 求生者 + 2 监管者技能可用，冷却与数值由 DA 驱动，改 DA 无需改代码 |
| AC-5 | 网络 | 150ms 延迟下翻窗/翻板不错位；破译进度各端误差 < 1%；无客户端可篡改伤害 |
| AC-6 | 架构 | 每个角色为独立 GameFeature 插件，禁用任一插件后其余功能正常 |
| AC-7 | 调试 | GAS 调试（GameplayInsights）+ `stat net` + 交互点状态可视化（GameplayDebugger 分类 `IDF`）可用 |

---

## 15. 里程碑与任务分解

| 阶段 | 名称 | 时长 `[待标定]` | 交付物 | 依赖 |
|---|---|---|---|---|
| M0 | 地基 | 1 周 | 模块与插件骨架、GAS 胶水（ASC/AbilitySet/InputConfig）、GameplayTag 表、日志与调试开关、CommonUI 壳 | — |
| M1 | 网络与流程 | 1.5 周 | GameMode/GameState/PlayerState/PlayerController、对局阶段状态机、Listen Server 5 端跑通、PlayerStart 分配 | M0 |
| M2 | 角色移动与动画 | 1.5 周 | `AIDFCharacterBase`、移动/蹲/跑、MotionWarping 翻越框架、TPP/FPP 双相机、PoseSearch 基础 | M1 |
| M3 | 地图灰盒 | 2 周 | L_MilitaryFactory 九分区、导航、出生点、机/椅/板/窗摆放、区域 Tag | M1 |
| M4 | 交互点玩法 | 2.5 周 | 密码机(含校准)、大门、椅子(含救援)、板、窗、地窖、宝箱、道具栏、交互提示 UI | M2, M3 |
| M5 | 角色实现 | 2.5 周 | 求生者公共（治疗/救援/道具）+ 4 求生者 + 监管者公共（攻击/擦刀/扛人/挂椅）+ 2 监管者 | M4 |
| M6 | 流程与 UI | 1.5 周 | 大厅/选人/匹配（本地）/对局 HUD/结算、音效与心跳、命中反馈 | M5 |
| M7 | AI 与打磨 | 2 周 | 监管者 AI（BT+EQS）、求生者 AI 补位、平衡调参、性能优化 | M6 |
| M8 | 测试与验收 | 1 周 | 自动化用例、Gauntlet 对局、网络压测、AC-1~AC-7 验证 | M7 |

**关键路径**：M0 → M1 → M2 → M4 → M5 → M6；M3 可与 M2 并行（关卡与角色互不阻塞）。
**风险缓冲**：整体预留 15% 时间；M5 是最大不确定项（红蝶瞬移与小丑冲刺的预测实现）。

---

## 16. 风险与对策

| 编号 | 风险 | 概率/影响 | 对策 |
|---|---|---|---|
| R1 | 红蝶瞬移/小丑冲刺的**预测与校正**做不好，导致"橡皮筋" | 高/高 | 位移一律走 `UCharacterMovementComponent` 自定义模式 + `FSavedMove`，禁止 `SetActorLocation`；先在 M2 做通用 Dash 框架并单测 |
| R2 | GAS 与交互点状态机边界混乱（把玩法塞进 GE） | 中/高 | 严守 ADR-2：交互点无 ASC；两侧只通过 GameplayTag + GameplayMessage 通信；Code Review 检查项 |
| R3 | 破译进度多端不一致引发信任问题 | 中/中 | 服务器时间为唯一真相；每 0.1s 复制 + 客户端平滑；自动化测试断言 5 端误差 < 1% |
| R4 | 军工厂布局与原作偏差过大，观感不对 | 中/中 | 先做**拓扑级**还原（分区/机位/板窗关系），再用截图对照迭代；建立布局对照文档 |
| R5 | 美术资产缺失导致长期灰盒 | 高/低 | 明确"占位优先"，使用 UE 商城免费/授权资产建立风格，不阻塞玩法验证 |
| R6 | UE5.8 新版本插件/API 变动（如 `ModularGameplayActors`、`GlobalDataLayer` 缺失） | 中/中 | 已规避：不依赖该类；所有插件启用前逐项在本机引擎目录验证（本设计已完成一次） |
| R7 | 数值平衡失控（监管者过强/过弱） | 高/中 | 数值全外置 DA + 一键导入 CSV 对照表；M7 集中调参；埋点统计每局破译/命中/救援数 |
| R8 | 范围蔓延（想加更多角色/地图） | 高/高 | 冻结第 1 轮范围为 1 图 6 角色；新需求进 backlog，不插入当前里程碑 |
| R9 | 版权风险 | 低/高 | 不发布、不商用、不使用原作资产；文档与工程内标注合规声明 |

---

## 17. 待确认问题清单（需你决策）

| 编号 | 问题 | 候选 | 我的建议 |
|---|---|---|---|
| Q1 | 对局规模是否固定 4v1，还是允许自定义（如 1v1/2v8） | A. 固定 4v1；B. 规则集支持自定义人数 | **A**（第 1 轮），`MatchRuleSet` 已预留人数字段 |
| Q2 | 监管者视角用第一人称还是第三人称 | A. 第一人称（贴近原作）；B. 第三人称（开发更快） | **A**，M2 同时搭好 FPP/TPP 双相机框架 |
| Q3 | 是否需要挣扎（QTE 挣脱椅子）完整实现 | A. 第 1 轮做简化版（延长计时）；B. 完整挣脱 | **A** |
| Q4 | 人格（Persona）天赋树是否本轮实现 | A. 不做，仅预留 Attribute 接口；B. 做精简版 | **A** |
| Q5 | 联机是否用 EOS/Steam 等在线子系统 | A. 本轮仅 IP 直连/局域网；B. 接入 OnlineSubsystem | **A**，第 3 轮再评估 |
| Q6 | AI 补位本轮做到什么程度 | A. 最小可用（能跑通验收）；B. 完整战术 AI | **A** |
| Q7 | 是否要手游触屏适配 | A. 不做；B. 做 | **A**（PC 优先） |
| Q8 | 数值以"贴近原作"还是"我们自己认为好玩"为准 | A. 贴近原作；B. 自调 | **A 为基线，B 为微调** |

---

## 18. 附录

### 18.1 目录结构约定（Content）

```
Content/IDF/
├── Maps/            L_MainMenu, L_MilitaryFactory, LI_Z1..Z9（Level Instance）
├── Characters/      Survivor/, Hunter/（骨骼、物理资产、ABP）
├── Abilities/       Common/, Survivor/, Hunter/, Items/
├── Effects/         GE_*, GC_*
├── Input/           IMC_*, IA_*
├── UI/              Common/(样式/按钮基类), HUD/, Lobby/, Result/
├── Data/            DA_*, DT_*
├── StateTree/       ST_*
├── AI/              BT_*, BB_*, EQS_*
├── Audio/           MetaSound/, Cues/, Modulations/
├── FX/              Niagara/
└── Art/             Props/, Environment/, Materials/
```

### 18.2 开发工具与调试

| 工具 | 用途 |
|---|---|
| `GameplayInsights` | GAS 时间线：技能激活、属性变化、GE 栈 |
| `GameplayDebugger`（自定义分类 `IDF`） | 运行时查看：交互点状态、破译进度、AI 黑板、GAS 标签 |
| `stat net` / `net.RepGraph` | 复制量与带宽 |
| `Unreal Insights` | CPU/GPU/网络全链路 |
| 控制台命令（自研） | `IDF.Debug.Machines 1`（显示机位）、`IDF.Match.ForceWin Survivor`、`IDF.Debug.Interact 1` |
| 自动化对局命令 | `IDF.Test.RunMatch -AI=4 -Duration=300`（Gauntlet 入口） |

### 18.3 术语表

| 术语 | 含义 |
|---|---|
| 求生者 | Survivor，4 人阵营，目标是破译逃脱 |
| 监管者 | Hunter，1 人阵营，目标是击倒并挂椅淘汰 |
| 密码机 | Cipher Machine，破译目标，7 台中完成 5 台 |
| 狂欢之椅 | Rocket Chair，挂人处，三阶段倒计时 |
| 板子 / 窗户 | Pallet / Window，转点（绕圈）交互物 |
| 大门电闸 | Gate Switch，通电后开启大门逃脱 |
| 地窖 | Basement / Hatch，最后一人逃脱手段 |
| 校准 | Calibration，破译中的 QTE |
| 擦刀 | Wipe Blade，监管者攻击后摇 |
| 恐惧半径 | Terror Radius，监管者接近时的心跳提示 |
| Gameplay Feature | 引擎的模块化玩法插件机制 |
| GAS | Gameplay Ability System，技能/属性/效果框架 |

### 18.4 参考对照（原作/引擎）

| 本设计概念 | 原作对应 | UE 实现对应 |
|---|---|---|
| 密码机 7/5 | 密码机 7 台破译 5 台 | `AIDMMachineActor` + StateTree |
| 狂欢之椅三阶段 | 火箭椅三阶段 | `UIDMChairTimerComponent` |
| 板/窗转点 | 木板与窗户 | `AIDFPalletActor` / `AIDFWindowActor` + MotionWarping |
| 医生医术 | 医生自疗 | `GA_Doctor_SelfHeal` + `GE_Doctor_HealBoost` |
| 幸运儿开箱 | 幸运儿必得道具 | `GA_LuckyGuy_Wish` + 服务器 Roll |
| 律师地图 | 律师侦查地图 | 按客户端过滤的 Intel RPC |
| 空军信号枪 | 信号枪眩晕 | 服务器权威投射物 + `GE_Hunter_Stunned` |
| 红蝶燕返 | 红蝶瞬移 | `GA_Geisha_Blink`（服务器裁定落点） |
| 小丑火箭冲刺 | 小丑冲刺 | CharacterMovement 自定义 Dash 模式 |
| 人格天赋 | 人格 | 预留 `AttributeSet` 扩展点（第 3 轮） |

---

## 19. 下一轮（第 2 轮）工作项预告

1. **搭建 M0 地基**：`Identity5.Build.cs` 依赖补全（GAS/ModularGameplay/GameplayInteractions/CommonUI/GameplayCameras 等）、`DefaultEngine.ini` 网络与地图配置、`DefaultGameplayTags.ini` 标签表、`IDFCore` 骨架（`AIDFCharacterBase`、`UIDFAbilitySystemComponent`、`UIDFAbilitySet`、`UIDFInputConfig`、`UIDFInteractionComponent` 接口）；
2. **`IDMMatch` 规则层**：`UIDMMatchRuleSet` 数据资产 + `AIDMGameState` 复制字段；
3. **`IDFCore` 交互抽象**：`IIDFInteractableTarget` + 交互提示 UI 打通（先用一台"假密码机"验证链路）；
4. 提交工程内 `README`（构建步骤 / 运行多客户端 PIE 的步骤）与 `.gitattributes`（LFS）。

> 本轮方案到此结束。第 2 轮见下方 `# 设计方案（第 2 轮）` 章节。

### 19.1 第 1 轮决策回填（已由项目负责人确认，第 2 轮以此为准）

| 编号 | 问题 | **最终决策** | 对本方案的影响 |
|---|---|---|---|
| D1 | 插件启用 | 由方案给出**确切清单**，由项目负责人在 UE 编辑器中开启 | 见第 2 轮「2.1 插件启用矩阵」，逐项标出插件名与模块名 |
| D2 | 对局规模 | **固定 4v1** | 人数不开放配置；`MatchRuleSet` 中的人数字段仅作常量校验 |
| D3 | 监管者视角 | **第三人称**（与求生者共用同一套相机框架，仅参数不同） | **第 1 轮 8.1 节"监管者第一人称"作废**；FPP 手臂动画与 FPP 专用骨骼全部取消，节省约 3~5 人日；`GameplayCameras` 仍需启用（受击摇晃、技能镜头、相机混合） |
| D4 | 椅子挣扎 | **第 1 轮简化版**：挣扎只延长当前阶段计时 | 完整挣脱机制移入 backlog（第 3 轮） |
| D5 | 人格天赋 | **不做**，仅预留 Attribute 接口 | 第 2 轮需定义「成长属性占位」（见第 2 轮 12 章），保证第 3 轮接入不改框架 |
| D6 | 联机方式 | **不打包、不做真联机匹配**；最终验收形态 = **UE 编辑器中同时打开 5 个 PIE 窗口联机测试** | 取消 Session/OnlineSubsystem/匹配服；新增「本地多窗口测试环境」专项（见第 2 轮 10 章）；`Listen Server + 多进程 PIE` 成为唯一目标拓扑 |
| D7 | AI 补位 | **最小可用**（能跑通验收即可） | 第 3 轮前只做"能破译 / 能追人"的最低智能 |
| D8 | 手游适配 | **不做** | 输入只做键鼠 + 手柄，不做触屏层 |
| D9 | 数值取向 | **贴近原作为基线 + 自行微调** | 所有 `[还原]` 值作为默认值落库，微调值通过 `DT_MatchTuning` 覆盖 |

> 以上 D1~D9 已在第 2 轮全部落实。第 1 轮正文中与之冲突的表述（监管者第一人称、真联机匹配、Persona 完整树）**以本节与第 2 轮为准**。
> 另有一处技术修正：第 1 轮 1.2 节推荐的 `GameplayInteractions` 插件，经核查在 UE5.8 中仍是**实验性**插件且硬依赖 SmartObjects/ContextualAnimation/NavCorridor，不适合本作交互模型，**第 2 轮起改用自研 `IIDFInteractableTarget` + GameplayStateTree**，详见第 2 轮 2.3 节。

---

# 设计方案（第 2 轮）

> **本轮主题**：M0 地基 + M1 网络与流程的**可编译骨架**设计。
> **本轮性质**：仍然是**设计**（接口、数据、时序、配置清单），不是成品代码。文中的代码块是**接口/字段级骨架**，用于固定接口契约，正式实现从第 3 轮开始。
> **前置输入**：第 1 轮方案 + 第 1 轮决策回填（D1~D9）。
> **本轮完成后的可验证结果**：在 UE 编辑器中打开 **5 个 PIE 窗口**（1 个 Listen Server + 4 个 Client），跳过大厅直接进入军工厂灰盒，各自控制一个占位角色，能互相看见移动、能被 ASC 授予一个空能力、能看到交互提示（对着一个假密码机）。

---

## 1. 本轮决策与范围

### 1.1 需你确认的决策（沿用第 1 轮 D1~D9，本轮不再重复提问）

| 编号 | 决策 | 本轮落地位置 |
|---|---|---|
| D1 | 插件清单由本文给出，你在编辑器中开启 | 2.1 插件启用矩阵 |
| D2 | 固定 4v1 | 12.1 `UIDMMatchRuleSet` 中人数为常量校验 |
| D3 | 监管者第三人称 | 13.3 相机规格（TPS 统一） |
| D4 | 挣扎简化版 | 本轮不实现，仅椅子状态机留 `bStruggling` 位 |
| D5 | 人格不做，仅预留 Attribute | 12.3 成长属性占位 |
| D6 | 不做真联机，只用 5 个 PIE 窗口 | 第 10 章 PIE 测试环境 |
| D7 | AI 最小可用 | 本轮不做，仅保留 `AIDFHunterAIController` 空类 |
| D8 | 不做手游 | 13.2 输入映射（键鼠 + 手柄） |
| D9 | 贴近原作 + 微调 | 12.4 数值落库与覆盖机制 |

### 1.2 本轮范围（冻结）

**做**：
- 工程骨架：插件与模块、Build.cs 依赖、`DefaultEngine.ini` / `DefaultGameplayTags.ini` / `DefaultInput.ini` / `DefaultEditorPerProjectUserSettings.ini`；
- `IDFCore`：`AIDFCharacterBase`、`IIDFInteractableTarget`、`UIDFInteractionComponent`、`UIDFInteractableStateComponent`、GameplayTag 原生注册、GAS 胶水（ASC / AbilitySet / InputConfig）、`UIDFNetClockComponent`、GameplayMessage 子系统；
- `IDMMatch`：`UIDMMatchRuleSet`、`AIDMGameMode`、`AIDMGameState`、`AIDMPlayerState`、`AIDMPlayerController`、本地选人占位流程、一个"假密码机"；
- `IDFUI`：HUD 壳 + 交互提示控件 + CommonUI 布局；
- 5 个 PIE 窗口的联机测试环境与验收脚本。

**不做（明确推迟）**：
- 密码机真实破译、大门、椅子救援、板、窗（M4）；
- 6 个角色技能（M5）；
- AI 行为（M7）；
- 大厅美术、匹配、结算（M6，且按 D6 简化为本地流程）；
- GameplayFeature 的"运行时按需加载"（本轮**预启用**全部插件，动态加载留到 M5 验证）。

### 1.3 本轮交付物

| 编号 | 交付物 | 验证方式 |
|---|---|---|
| R2-1 | `Identity5.uproject` 插件启用后的可编译工程 | 编辑器能打开、能编译 |
| R2-2 | `IDFCore` / `IDMMatch` 两个 GameFeature 插件骨架 | 插件能加载，类能被蓝图继承 |
| R2-3 | 5 窗口联机 + 角色生成 + 移动同步 | 手动验收 AC-R2 |
| R2-4 | 交互提示链路（假密码机） | 靠近显示提示、离开消失、按 F 服务器收到请求 |
| R2-5 | GameplayTag 表 + 校验命令 | `IDF.Debug.Tags.Validate` 无报错 |
| R2-6 | 本地测试说明 `Documents/testing_pie.md` | 照文档能复现 5 窗口 |

---

## 2. 工程骨架

### 2.1 插件启用矩阵（D1：请你在编辑器中开启）

**结论先说**：需要你开启 **11 个插件**；其余在 5.8 中默认已启用，我逐项核实过。

**A. 必须由你手动开启（11 个，在 编辑器 → 编辑 → 插件 中搜索并勾选，然后重启编辑器）**

| # | 编辑器中的插件名 | 用途 | 是否需重启 | 备注 |
|---|---|---|---|---|
| 1 | **Gameplay Abilities** | GAS 主体 | 是 | 5.8 默认关闭，必须开 |
| 2 | **Game Features** | Gameplay Feature 插件机制 | 是 | 会自动带上 Modular Gameplay / Data Registry |
| 3 | **Modular Gameplay** | `UGameFrameworkComponentManager` 扩展点 | 是 | 通常被 2 自动开启，确认已勾选 |
| 4 | **Common UI** | 跨平台 UI 与输入路由 | 是 | 会自动带上 Enhanced Input |
| 5 | **State Tree** | 交互点/角色状态机 | 是 | 会自动带上 Gameplay Insights |
| 6 | **Gameplay State Tree** | `UStateTreeComponent` / `UStateTreeAIComponent`（AI 与交互点用） | 是 | ⚠️ 这两个组件类**不在 State Tree 插件里**，必须单独开这个 |
| 7 | **Motion Warping** | 翻窗/翻板/救援的位移对齐（M2/M4 用） | 是 | Beta 但稳定 |
| 8 | **Pose Search** | 姿势搜索选动画（追击急转） | 是 | 会自动带上 Chooser / Blend Stack / Animation Warping |
| 9 | **Audio Modulation** | 心跳/追击音乐动态混音 | 是 | — |
| 10 | **Sound Utilities** | 音效工具与 MetaSound 辅助 | 是 | — |
| 11 | **Gameplay Insights** | GAS 调试时间线（开发期必备） | 是 | 会被 State Tree 自动带上，确认已勾选 |

**B. 默认已启用，无需操作（我核实过 5.8 默认开启）**

| 插件名 | 用途 |
|---|---|
| Enhanced Input | 输入系统 |
| Gameplay Cameras | 相机（受击摇晃、技能镜头） |
| Niagara | 特效 |
| MetaSound | 音频合成 |
| Sound Fields | 空间音频 |
| Data Validation | 数据资产校验 |
| Control Rig / IK Rig | 绑定与 IK |
| Modeling Tools Editor Mode | 灰盒建模（你工程里已启用） |

**C. 明确不启用的插件（及理由）**

| 插件名 | 不启用理由 |
|---|---|
| **Gameplay Interactions** | 5.8 中仍标记为 **Experimental**，且硬依赖 SmartObjects + ContextualAnimation + NavCorridor（实验性组合）。它是"棋盘式智能交互（找槽位/占位）"模型，与本作"板/窗/机位固定交互"模型不匹配，且会引入 3 个实验性插件的维护面。我们改用自研 `IIDFInteractableTarget` + 普通 `UStateTreeComponent`（见 2.3）。 |
| **Replication Graph** | 5 人房间用不上，属性复制开销可忽略；留作压测备选。 |
| **Iris** | 同上，实验性，5 人规模无收益。 |
| **Mass Entity 全套** | 无大量 AI 单位需求。 |
| **GlobalDataLayer** | 该插件在 UE5.8 引擎目录中**不存在**（我逐目录搜索确认），第 1 轮已将其列为"未发现"，本轮正式排除。 |

**D. 如果某个 GameFeature 插件不在列表里（排查用）**

| 现象 | 原因 | 处理 |
|---|---|---|
| 找不到 "Gameplay State Tree" | 它属于 `Plugins/Runtime/GameplayStateTree`，在插件面板按 "Gameplay" 分类找 | 用搜索框输入 "State Tree" 会同时列出 State Tree 与 Gameplay State Tree |
| 勾选 Gameplay Abilities 后编译报缺 `GameplayTags` | `GameplayTags` 是**引擎内置模块**（`Engine/Source/Runtime/GameplayTags`），不是插件 | 只需在 `Build.cs` 里加依赖，无需开插件 |
| 勾选后 C++ 编译报缺 `SmartObjects` | 说明误开了 Gameplay Interactions | 关掉它 |

> 开启插件后 `.uproject` 会由编辑器自动写入 `"Plugins"` 数组；**第 3 轮我会给出 `.uproject` 的最终内容**并与编辑器写入结果对账。

### 2.2 模块划分与 Build.cs 依赖（本轮定稿）

> 📌 **实施期修正（S05 落地后同步）**：**GAS 三件套（`GameplayAbilities` / `GameplayTags` / `GameplayTasks`）作为依赖应加在 `IDFCore` 插件上**，而不是主模块——因为 GAS 骨架（ASC / AttributeSet / AbilitySet）按 2.1 节归属 `IDFCore`。主模块只通过 **`PrivateDependencyModuleNames` 依赖 `IDFCore`**。下面的清单保留了主模块的 GAS 依赖（无害，将来主模块自己写 GAS 代码时用得上），但**必须同时有 `IDFCore` 依赖**。
>
> 另外统一一条目录约定：主模块的源文件按功能分子目录（`Public/` 与 `Private/` 对称）。**GameMode 相关的一律放 `GameMode/`**（不放 `GameFramework/`，避免与引擎自带的 `Engine/Source/Runtime/Engine/Classes/GameFramework/` 在阅读时混淆）。

**主模块 `Source/Identity5/Identity5.Build.cs`**

```csharp
PublicDependencyModuleNames.AddRange(new string[] {
    "Core", "CoreUObject", "Engine", "InputCore",
    "EnhancedInput",
    "GameplayTags",            // 引擎内置模块：FGameplayTag / FNativeGameplayTag
    "GameplayAbilities",       // GAS（主模块自己写 GAS 代码时用）
    "GameplayTasks",           // GAS 的依赖（能力任务）
    "ModularGameplay",         // UGameFrameworkComponentManager
    "GameFeatures",            // UGameFeaturesSubsystem
    "CommonUI", "CommonInput", // UI 与输入路由
    "GameplayCameras",         // 相机
    "StateTreeModule",         // UStateTree / 资产引用
    "GameplayStateTreeModule", // UStateTreeComponent（挂 Pawn/Actor）
    "UMG", "Slate", "SlateCore"
});

PrivateDependencyModuleNames.AddRange(new string[] {
    "IDFCore", "IDMMatch"      // 通过插件依赖，见 2.4 的单向依赖规则
});
```

**`IDFCore.Build.cs`**（GameFeature 插件）

```csharp
PublicDependencyModuleNames.AddRange(new string[] {
    "Core", "CoreUObject", "Engine", "InputCore",
    "GameplayTags", "GameplayAbilities", "GameplayTasks",
    "EnhancedInput", "ModularGameplay", "GameFeatures",
    "CommonUI", "CommonInput", "UMG",
    "GameplayCameras",
    "StateTreeModule", "GameplayStateTreeModule",
    "MotionWarping",           // 交互位移对齐（接口层先引入，实现放 M2）
    "DeveloperSettings"        // UIDFDeveloperSettings（调试开关）
});
```

**`IDMMatch.Build.cs`**

```csharp
PublicDependencyModuleNames.AddRange(new string[] {
    "Core", "CoreUObject", "Engine",
    "GameplayTags", "GameplayAbilities", "GameplayTasks",
    "IDFCore",
    "StateTreeModule", "GameplayStateTreeModule",
    "AIModule", "NavigationSystem",   // M7 的 AI 预留
    "DeveloperSettings"
});
```

**`IDFUI.Build.cs`**

```csharp
PublicDependencyModuleNames.AddRange(new string[] {
    "Core", "CoreUObject", "Engine",
    "CommonUI", "CommonInput", "UMG", "Slate", "SlateCore",
    "GameplayTags", "ModelViewViewModel",   // UMG ViewModel（第 1 轮 11.3 的数据流）
    "IDFCore", "IDMMatch"
});
```

**`IDFChar_*.Build.cs` / `IDFMap_*.Build.cs`**（第 3 轮起）

```csharp
PublicDependencyModuleNames.AddRange(new string[] {
    "Core", "CoreUObject", "Engine",
    "GameplayTags", "GameplayAbilities", "GameplayTasks",
    "EnhancedInput", "IDFCore", "IDMMatch"
    // 需要时追加：MotionWarping / Niagara / GameplayCameras
});
```

**动画相关（M2 再加，本轮不引入以免空依赖）**：`AnimationWarpingRuntime`、`AnimationLocomotionLibraryRuntime`、`PoseSearch`、`MotionTrajectory`、`Chooser`。

> ⚠️ **命名陷阱（已核实）**：这几个模块名与插件名**不一致**，写错就编译失败：
> `GameplayStateTreeModule`（不是 `GameplayStateTree`）、`GameplayInteractionsModule`、`AnimationWarpingRuntime`（不是 `AnimationWarping`）、`AnimationLocomotionLibraryRuntime`、`MetasoundEngine`（不是 `Metasound`）。

### 2.3 与第 1 轮的技术修正：弃用 GameplayInteractions

| 项 | 第 1 轮方案 | 第 2 轮修正 | 理由 |
|---|---|---|---|
| 交互实现 | GameplayInteractions + StateTree | **自研 `IIDFInteractableTarget` + `UIDFInteractableStateComponent`（内部跑 StateTree）** | GameplayInteractions 是 Experimental，依赖 SmartObjects(Experimental)/ContextualAnimation(Experimental)/NavCorridor(Experimental)；它的 `FGameplayInteractionContext`/`FindSlotEntranceLocation` 面向"智能对象找槽位"，本作交互点是固定槽位，收益为负 |
| 状态机载体 | `UGameplayInteractionUserComponent` | `UStateTreeComponent`（GameplayStateTree 插件） | 组件在 `GameplayStateTree` 插件里，非 StateTree 插件，已在 2.1 标注 |
| 保留的收益 | 状态机可视化/可配置 | 不变 | StateTree 仍然是交互状态机的实现载体 |

### 2.4 目录与文件清单（本轮要落地的文件）

```
Identity5.uproject                         ← 插件启用（编辑器写入）
Config/
├── DefaultEngine.ini                      ← 增补：默认地图、GameMode、网络、PIE 相关
├── DefaultGameplayTags.ini                ← 新建：标签表（原生标签的兜底与编辑器可见性）
├── DefaultInput.ini                       ← 增补：默认输入（Enhanced Input 为主）
├── DefaultGame.ini                        ← 增补：项目名、版本
└── DefaultEditorPerProjectUserSettings.ini ← 新建：PIE 5 窗口 + 网络模拟（第 10 章）
Plugins/GameFeatures/
├── IDFCore/
│   ├── IDFCore.uplugin
│   ├── IDFCore.Build.cs
│   └── Source/IDFCore/
│       ├── Public/  (见 3~9 章的头文件)
│       └── Private/
└── IDMMatch/
    ├── IDMMatch.uplugin
    ├── IDMMatch.Build.cs
    └── Source/IDMMatch/
        ├── Public/
        └── Private/
Documents/
├── design_analysis.md                     ← 本文件
├── testing_pie.md                         ← 新建：5 窗口测试步骤
└── plugin_setup.md                        ← 新建：插件开启清单与截图步骤
```

---

## 3. `IDFCore`：角色基类

### 3.1 `AIDFCharacterBase`（骨架）

```cpp
// IDFCore/Public/Characters/IDFCharacterBase.h
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/GameplayMessageSubsystem.h"   // 若采用引擎自带子系统
#include "IDFCharacterBase.generated.h"

class UIDFAbilitySystemComponent;
class UIDFAbilitySet;
class UIDFInputConfig;
class UIDFInteractionComponent;
class UIDFNetClockComponent;
class UAttributeSet;

UENUM(BlueprintType)
enum class EIDFTeam : uint8
{
    None      UMETA(DisplayName = "无"),
    Survivor  UMETA(DisplayName = "求生者"),
    Hunter    UMETA(DisplayName = "监管者")
};

UCLASS(Abstract)
class IDFCORE_API AIDFCharacterBase : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()
public:
    AIDFCharacterBase();

    //~ IAbilitySystemInterface
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;   // 返回 IDFASC

    UFUNCTION(BlueprintPure, Category = "IDF|Team")
    EIDFTeam GetTeam() const { return Team; }

    UFUNCTION(BlueprintPure, Category = "IDF|Team")
    bool IsSurvivor() const { return Team == EIDFTeam::Survivor; }

    // 装配（由 GameMode / 角色 Feature 调用）
    UFUNCTION(BlueprintCallable, Category = "IDF|Ability")
    void GiveAbilitySet(UIDFAbilitySet* AbilitySet);

    // 仅服务器：生命状态切换（M4 起由 GE 驱动，本轮只做标签）
    UFUNCTION(BlueprintCallable, Category = "IDF|State")
    void SetSurvivorLifeState(FGameplayTag NewStateTag);

protected:
    virtual void BeginPlay() override;
    virtual void PossessedBy(AController* NewController) override;   // 服务器：ASC 初始化
    virtual void OnRep_PlayerState() override;                       // 客户端：ASC 初始化兜底

    //~ 装配
    virtual void InitializeAbilitySystem();
    virtual void ApplyAbilitySet();

    //~ 移动参数（默认值，可在蓝图/DA 覆盖）
    UPROPERTY(EditDefaultsOnly, Category = "IDF|Movement")
    float WalkSpeed = 400.f;              // 4.0 m/s  [还原/待标定]

    UPROPERTY(EditDefaultsOnly, Category = "IDF|Movement")
    float SprintSpeed = 550.f;            // 5.5 m/s  [待标定]

    UPROPERTY(EditDefaultsOnly, Category = "IDF|Movement")
    float CrouchSpeed = 200.f;            // 2.0 m/s  [待标定]

    //~ 组件
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "IDF|Components")
    TObjectPtr<UIDFAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "IDF|Components")
    TObjectPtr<UIDFInteractionComponent> InteractionComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "IDF|Components")
    TObjectPtr<UIDFNetClockComponent> NetClockComponent;

    //~ 属性集（蓝图子类里填具体 AttributeSet 类与默认 GE）
    UPROPERTY(EditDefaultsOnly, Category = "IDF|Ability")
    TArray<TSubclassOf<UAttributeSet>> DefaultAttributeSets;

    UPROPERTY(EditDefaultsOnly, Category = "IDF|Ability")
    TObjectPtr<UIDFAbilitySet> DefaultAbilitySet;

    UPROPERTY(EditDefaultsOnly, Category = "IDF|Input")
    TObjectPtr<UIDFInputConfig> InputConfig;

    UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadOnly, Category = "IDF|Team")
    EIDFTeam Team = EIDFTeam::None;

    UPROPERTY(ReplicatedUsing = OnRep_LifeStateTag, BlueprintReadOnly, Category = "IDF|State")
    FGameplayTag LifeStateTag;      // State.Survivor.* / State.Hunter.*

    UFUNCTION()
    void OnRep_LifeStateTag();

    bool bAbilitySystemInitialized = false;
};
```

**关键约定**：

| 约定 | 说明 |
|---|---|
| ASC 归属 | 第 1 轮决定：**挂 Character**（本轮无重生需求）。`GetAbilitySystemComponent()` 内部做"Character 有就返回，否则查 PlayerState"的兜底，为第 3 轮迁移留路 |
| 初始化时机 | 服务器：`PossessedBy` → `InitializeAbilitySystem`；客户端：`OnRep_PlayerState` → 同一函数；用 `bAbilitySystemInitialized` 防重入 |
| 初始化顺序 | `InitAbilityActorInfo(this, this)` → 添加 AttributeSet → 应用 `DefaultAbilitySet` → 加 GameplayTag `State.Ready` → 广播 `Event.Character.Ready`（UI 与 AI 以此为"可交互"信号） |
| 移动参数 | 本轮用 `MaxWalkSpeed` 的默认值；M5 起改由 `MoveSpeedMultiplier` 属性驱动（GAS 乘区） |
| 跳跃 | **关闭**（`bCanJump=false`）：原作无跳跃，避免多余状态。翻越走专用能力 |
| 网络 | `SetReplicateMovement(true)`（`ACharacter` 默认）、`bReplicates = true`、`NetUpdateFrequency` 在基类构造里设为 **30**、`MinNetUpdateFrequency = 10` |

### 3.2 `AIDFSurvivorCharacter` / `AIDFHunterCharacter`（派生占位）

```cpp
// IDFSurvivor 插件（第 3 轮起）：本轮先在 IDFCore 里放两个占位派生类，避免提前建插件
UCLASS(Abstract) class AIDFSurvivorCharacter : public AIDFCharacterBase { /* Team=Survivor；受击加速、治疗接口占位 */ };
UCLASS(Abstract) class AIDFHunterCharacter  : public AIDFCharacterBase { /* Team=Hunter；攻击接口占位 */ };
```

> 说明：第 1 轮设计里 `IDFSurvivor` / `IDFHunter` 是独立 GameFeature 插件。本轮为避免"插件之间空依赖"，先放 `IDFCore` 内，**第 3 轮建插件时整体迁移**（迁移只改 `UCLASS` 所在的模块与 Build.cs，不影响任何接口契约）。

---

## 4. `IDFCore`：交互抽象（本轮的验证重点）

### 4.1 交互类型枚举与选项结构

```cpp
// IDFCore/Public/Interaction/IDFInteractionTypes.h
UENUM(BlueprintType)
enum class EIDFInteractionType : uint8
{
    Hold        UMETA(DisplayName = "持续按住（破译/治疗/开门/救援）"),
    Press       UMETA(DisplayName = "单次按压（翻窗/翻板/开箱/跳地窖）"),
    Toggle      UMETA(DisplayName = "开关（电闸总开关，预留）")
};

UENUM(BlueprintType)
enum class EIDFInteractionAvailability : uint8
{
    Available       UMETA(DisplayName = "可用"),
    Unavailable     UMETA(DisplayName = "不可用（灰显 + 原因）"),
    Hidden          UMETA(DisplayName = "不显示（条件不满足，如未通电的大门）")
};

USTRUCT(BlueprintType)
struct IDFCORE_API FIDFInteractionOption
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTag OptionTag;         // Interact.Machine.Decode
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText      DisplayName;         // "破译密码机"
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText      ActionVerb;          // "按住"
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EIDFInteractionType Type = EIDFInteractionType::Hold;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float      HoldDuration = 0.f;  // Hold 型：0 表示"持续型"，进度由目标自算
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32      Priority = 0;         // 同一目标多选项时的排序
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTag UnavailableReason; // Interact.Reason.Decoded
    UPROPERTY(BlueprintReadOnly)                EIDFInteractionAvailability Availability = EIDFInteractionAvailability::Available;
    UPROPERTY(BlueprintReadOnly)                FTransform InteractionTransform; // 角色应站的位置（MotionWarping 用）
    UPROPERTY(BlueprintReadOnly)                TWeakObjectPtr<AActor> TargetActor;
    UPROPERTY(BlueprintReadOnly)                int32      TargetOptionIndex = 0;
};
```

### 4.2 `IIDFInteractableTarget` 接口

```cpp
// IDFCore/Public/Interaction/IDFInteractableTarget.h
UINTERFACE(MinimalAPI, BlueprintType)
class UIDFInteractableTarget : public UInterface { GENERATED_BODY() };

class IDFCORE_API IIDFInteractableTarget
{
    GENERATED_BODY()
public:
    // 目标向交互者描述"我现在能提供哪些交互"
    virtual void GatherInteractionOptions(const FIDFInteractionQuery& Query, TArray<FIDFInteractionOption>& OutOptions) = 0;

    // 服务器与客户端都会调用（客户端只用来起本地表现）
    virtual void OnInteractionBegin(const FIDFInteractionRequest& Request) {}
    virtual void OnInteractionUpdate(const FIDFInteractionRequest& Request, float DeltaSeconds) {}
    virtual void OnInteractionEnd(const FIDFInteractionRequest& Request, bool bWasCancelled) {}

    // 服务器校验的唯一入口：返回 false 时必须在 OutReason 里写出原因（仅服务器日志用，不下发客户端）
    virtual bool CanInteract(const FIDFInteractionRequest& Request, FGameplayTag& OutReason) const { return true; }

    // UI：是否对指定交互者可见（律师地图、隐藏交互点等按客户端过滤时用）
    virtual bool IsInteractableVisibleFor(const AActor* Interactor) const { return true; }
};

// 查询/请求结构（同一份结构客户端与服务器共用，便于"客户端预测、服务器裁定"）
USTRUCT(BlueprintType)
struct IDFCORE_API FIDFInteractionQuery
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AActor> Interactor;
    UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AActor> TargetActor;
    UPROPERTY(BlueprintReadOnly) FGameplayTagContainer InteractorTags;   // 用于条件判断（如"受伤才能被治疗"）
};

USTRUCT(BlueprintType)
struct IDFCORE_API FIDFInteractionRequest
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AActor> Interactor;      // 发起者
    UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AActor> TargetActor;     // 交互点
    UPROPERTY(BlueprintReadOnly) FGameplayTag           OptionTag;       // 选择了哪个选项
    UPROPERTY(BlueprintReadOnly) int32                  OptionIndex = 0;
    UPROPERTY(BlueprintReadOnly) int32                  SequenceId  = 0;  // 防重放/错序
    UPROPERTY(BlueprintReadOnly) FTransform             InteractorTransform; // 供服务器距离校验
};
```

### 4.3 `UIDFInteractionComponent`：检测 → 预测 → 服务器裁定

**职责边界（务必守住）**：

| 端 | 职责 |
|---|---|
| 客户端 | 用 Trace 找候选目标 → 组装交互选项 → 驱动 UI 提示 → 本地播放"开始读条"表现 → 发 `Server RPC` |
| 服务器 | **不信任客户端的任何参数**：重新查询目标、重算选项、`CanInteract` 校验（距离 ≤ `MaxInteractDistance + 容差`、朝向、状态标签、冷却、互斥）→ 通过才 `OnInteractionBegin` → 广播表现 |
| 双方的契约 | 通过 `SequenceId` + `OptionTag` 对齐；服务器拒绝时回 `Client RPC` 让客户端停止表现（不回滚位置，位置由 CharacterMovement 自己校正） |

```cpp
// IDFCore/Public/Interaction/IDFInteractionComponent.h
UCLASS(ClassGroup=(IDF), meta=(BlueprintSpawnableComponent))
class IDFCORE_API UIDFInteractionComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UIDFInteractionComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    //~ 输入入口（由 EnhancedInput 的 IA_Interact / IA_InteractHold 调用，在 AIDMPlayerController 或 Character 里绑定）
    UFUNCTION(BlueprintCallable, Category="IDF|Interaction")
    void OnInteractPressed();
    UFUNCTION(BlueprintCallable, Category="IDF|Interaction")
    void OnInteractReleased();

    //~ UI 只读接口
    UFUNCTION(BlueprintPure, Category="IDF|Interaction")
    const FIDFInteractionOption& GetBestOption() const { return BestOption; }
    UFUNCTION(BlueprintPure, Category="IDF|Interaction")
    bool HasValidOption() const { return BestOption.Availability == EIDFInteractionAvailability::Available; }
    UFUNCTION(BlueprintPure, Category="IDF|Interaction")
    float GetLocalHoldProgress() const;   // 0..1，本地预测表现用

    // 供 UI 订阅（进度/提示变化）
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInteractionUIChanged);
    UPROPERTY(BlueprintAssignable) FOnInteractionUIChanged OnInteractionUIChanged;

protected:
    //~ 客户端扫描
    void ScanForInteractables();
    void SelectBestOption(const TArray<FIDFInteractionOption>& Candidates);

    //~ 服务器校验
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_BeginInteraction(FGameplayTag OptionTag, AActor* TargetActor, int32 SequenceId, FTransform InteractorTransform);
    UFUNCTION(Server, Reliable)
    void Server_EndInteraction(int32 SequenceId, bool bCancelled);

    //~ 裁定回执（客户端停止本地表现）
    UFUNCTION(Client, Reliable)
    void Client_InteractionRejected(int32 SequenceId, FGameplayTag Reason);
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_InteractionStarted(AActor* Interactor, AActor* TargetActor, FGameplayTag OptionTag);
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_InteractionEnded(AActor* Interactor, FGameplayTag OptionTag, bool bCancelled);

    //~ 参数（全部可在 DA/蓝图覆盖）
    UPROPERTY(EditDefaultsOnly, Category="IDF|Interaction") float ScanInterval        = 0.1f;
    UPROPERTY(EditDefaultsOnly, Category="IDF|Interaction") float ScanRadius          = 220.f;   // 2.2 m
    UPROPERTY(EditDefaultsOnly, Category="IDF|Interaction") float MaxInteractDistance = 250.f;   // 服务器校验上限
    UPROPERTY(EditDefaultsOnly, Category="IDF|Interaction") float MaxDistanceTolerance= 120.f;   // 延迟容差（约 1.5 个身位）
    UPROPERTY(EditDefaultsOnly, Category="IDF|Interaction") float MaxFacingDot        = -0.2f;   // 朝向点积下限（约 100° 锥）

    UPROPERTY(BlueprintReadOnly, Replicated) FGameplayTag ActiveOptionTag;   // 当前正在进行的交互（仅 UI/表现用）
    UPROPERTY(BlueprintReadOnly, Replicated) TObjectPtr<AActor> ActiveTarget;
    UPROPERTY(BlueprintReadOnly) int32 NextSequenceId = 0;
    UPROPERTY(BlueprintReadOnly) FIDFInteractionOption BestOption;
};
```

**扫描实现要点（设计级）**：

1. 用 `SweepMultiByObjectType`（球形，半径 `ScanRadius`，`ECC_WorldDynamic` + 自定义 `IDF_Interactable` 通道）而不是 `OverlapMulti`：避免穿透薄墙提示隔壁的密码机；
2. 只保留实现了 `IIDFInteractableTarget` 的 Actor，并调用 `IsInteractableVisibleFor(this)` 过滤；
3. 用「距离 + 视线 (`LineOfSightTo`) + 优先级」排序取最优；
4. 扫描频率 0.1s，且**只在结果变化时**刷新 UI（避免每帧 UI 抖动）；
5. 交互中不再重新选择目标（锁定 `ActiveTarget`），直到 `End`。

**服务器校验清单（`CanInteract` 之前的一道闸）**：

| 校验 | 阈值/条件 | 失败处理 |
|---|---|---|
| 距离 | `Dist2D(Interactor, Target) <= MaxInteractDistance + MaxDistanceTolerance` | 拒绝，记 `LogIDFInteraction` Verbose |
| 朝向 | `FVector::DotProduct(Facing, ToTarget) >= MaxFacingDot` | 拒绝 |
| 目标仍存在且实现接口 | `TargetActor->Implements<UIDFInteractableTarget>()` | 拒绝 |
| 选项仍有效 | 服务器重新 `GatherInteractionOptions` 且含该 `OptionTag` | 拒绝 |
| 目标自校验 | `Target->CanInteract(Request, Reason)` | 拒绝并记录 Reason |
| 交互者状态 | 无 `State.Downed / State.Carried / State.OnChair / State.Stunned` 等阻断标签 | 拒绝 |
| 互斥 | 无 `State.Interacting.*` 或该交互允许并发（密码机允许多人） | 拒绝 |

### 4.4 交互时序（含预测与拒绝）

```
客户端                                          服务器
  │ 每 0.1s Trace 扫描                              │
  │ 选中 BestOption → UI 显示"按住 F 破译"            │
  │ 玩家按住 F                                       │
  │ ├─ 本地：进度环开始转（预测表现）                   │
  │ └─ Server_BeginInteraction(Tag, Target, SeqId, Xform) ──▶
  │                                                 │ 重新 GatherOptions
  │                                                 │ 距离/朝向/状态/CanInteract 校验
  │                                                 │ ├─ 通过：
  │                                                 │ │   Target->OnInteractionBegin(Request)
  │                                                 │ │   Multicast_InteractionStarted(...) ──▶ 所有客户端播表现
  │                                                 │ └─ 失败：
  │                                                 │     Client_InteractionRejected(SeqId, Reason) ──▶ 停止本地表现
  │ 玩家松开 F                                       │
  │ └─ Server_EndInteraction(SeqId, false) ─────────▶ Target->OnInteractionEnd(Request, false)
  │                                                 │ Multicast_InteractionEnded(...) ──▶ 所有客户端停止表现
  │ 被监管者命中（Event.Combat.Hit）                  │
  │ └─ 本地立即中断表现                               │ 服务器 CancelAbilitiesByTag → OnInteractionEnd(cancelled)
```

**为什么"进度条"放在客户端预测**：按住类交互的即时反馈决定手感。服务器仍然按自己的 `DeltaTime` 计算权威进度，客户端进度条在 0.25s 内插值到服务器值（第 1 轮 ADR-7 的落地方案）。

### 4.5 `UIDFInteractableStateComponent`（交互点侧共用基类）

```cpp
// IDFCore/Public/Interaction/IDFInteractableStateComponent.h
UCLASS(Abstract, ClassGroup=(IDF), meta=(BlueprintSpawnableComponent))
class IDFCORE_API UIDFInteractableStateComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintPure, Category="IDF|Interactable")
    bool IsOccupied() const { return Interactors.Num() > 0; }

    UFUNCTION(BlueprintPure, Category="IDF|Interactable")
    int32 GetInteractorCount() const { return Interactors.Num(); }

protected:
    // 服务器：注册/注销交互者，并维护"占用集合"
    void AddInteractor(AActor* Interactor);
    void RemoveInteractor(AActor* Interactor);

    // StateTree 事件入口（供 ST_Machine 等资产使用）
    void SendStateTreeEvent(const FGameplayTag& EventTag);

    UPROPERTY(Replicated) float          Progress01     = 0.f;    // 0..1，离散复制（0.1s 或 >0.5%）
    UPROPERTY(Replicated) bool           bCompleted     = false;
    UPROPERTY(ReplicatedUsing=OnRep_StateTag) FGameplayTag StateTag; // Interact.State.Decoding 等
    UPROPERTY() TArray<TWeakObjectPtr<AActor>> Interactors;         // 仅服务器持有
    UPROPERTY(EditDefaultsOnly, Category="IDF|Interactable") TObjectPtr<UStateTree> StateTreeAsset;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStateTreeComponent> StateTreeComponent;  // GameplayStateTree 插件
};
```

**假密码机（本轮验收载体）**：`AIDFDebugMachineActor`

| 项 | 规格 |
|---|---|
| 实现接口 | `IIDFInteractableTarget` |
| 选项 | `Interact.Machine.Decode`（Hold，持续型） |
| 行为 | 服务器按 `DecodeRate * DeltaTime` 累加 `Progress01`；达到 1.0 时 `bCompleted=true`、广播 `Event.Machine.Decoded`、选项变为 `Hidden` |
| UI | 客户端显示"按住 F 破译" + 进度环；进度来自复制值 |
| 复制 | `Progress01` 每 0.1s 或变化 >0.5%；`bCompleted` `RepNotify` |
| 调试 | `IDF.Debug.Interact 1` 在屏幕上打印当前所有交互点状态 |

> 这台"假密码机"不是废弃代码：M4 的真实 `AIDMMachineActor` 直接继承它的状态机组件，把它替换成完整版（含校准 QTE、多人协作）。

---

## 5. GameplayTag 体系（原生注册）

### 5.1 注册机制（第 1 轮只给了标签名，这里定死实现方式）

| 机制 | 用途 | 说明 |
|---|---|---|
| `UE_DECLARE_GAMEPLAY_TAG_EXTERN` + `UE_DEFINE_GAMEPLAY_TAG` | C++ 原生标签（**首选**） | 编译期检查拼写、零字符串查找、无需 ini；已核实 API 存在于 `NativeGameplayTags.h` |
| `Config/DefaultGameplayTags.ini` | 兜底与编辑器可见性 | 让策划在编辑器里能看到/搜索标签；**不要**在这里和原生标签重复定义同名标签（会警告） |
| GameplayTag 表资产（DataTable） | 不可行 | 本作标签全在 C++ 侧声明，不需要 |

**标签命名空间规划（按模块归属，避免冲突）**：

| 前缀 | 归属模块 | 示例 |
|---|---|---|
| `Ability.*` | 各角色插件 / IDFCore | `Ability.Survivor.Decode` |
| `State.*` | IDFCore | `State.Survivor.Injured` |
| `Event.*` | IDFCore / IDMMatch | `Event.Machine.Decoded` |
| `Interact.*` | IDFCore | `Interact.Machine.Decode` |
| `Input.*` | IDFCore | `Input.Interact`, `Input.Skill1` |
| `Match.*` | IDMMatch | `Match.Phase.Playing` |
| `Region.*` | IDFMap_* | `Region.Z1.CentralFactory` |
| `UI.*` | IDFUI | `UI.Layer.Game` |

### 5.2 声明与定义骨架

```cpp
// IDFCore/Public/IDFGameplayTags.h
#pragma once
#include "NativeGameplayTags.h"

namespace IDFTags
{
    // ── 阵营/通用状态 ──
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Ready);
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);

    // ── 求生者状态（第 1 轮 9.3 的核心子集）──
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Survivor_Healthy);
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Survivor_Injured);
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Survivor_Downed);
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Survivor_Carried);
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Survivor_OnChair_Phase1);
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Survivor_OnChair_Phase2);
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Survivor_Escaped);
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Survivor_Eliminated);

    // ── 监管者状态 ──
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Hunter_Chasing);
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Hunter_Attacking);
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Hunter_Stunned);
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Hunter_Carrying);

    // ── 交互（本轮重点）──
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact_Machine_Decode);
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact_Reason_Decoded);
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact_Reason_Occupied);
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Interacting_Decode);

    // ── 输入 ──
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Interact);
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_InteractHold);
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Skill1);
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Sprint);
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Crouch);

    // ── 事件（GameplayMessage 用）──
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Character_Ready);
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Interaction_Rejected);
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Machine_Decoded);
}

// IDFCore/Private/IDFGameplayTags.cpp
#include "IDFGameplayTags.h"

namespace IDFTags
{
    UE_DEFINE_GAMEPLAY_TAG(State_Ready,                 "State.Ready");
    UE_DEFINE_GAMEPLAY_TAG(State_Survivor_Healthy,     "State.Survivor.Healthy");
    // ... 其余逐个对应
}
```

### 5.3 标签校验（自动化友好）

| 校验项 | 规则 | 实现 |
|---|---|---|
| 命名规范 | 根命名空间必须在白名单（`Ability/State/Event/Interact/Input/Match/Region/UI`） | `IDF.Debug.Tags.Validate` 控制台命令 + Automation Test |
| 层级深度 | `State.Survivor.OnChair.Phase1` 这类"状态"标签深度 ≤ 5，便于 `MatchesTag` 通配 | 同上 |
| 互斥对 | 同一容器内不得同时存在 `State.Survivor.Healthy` 与 `State.Survivor.Injured` | 运行时断言（`ensureMsgf`） |
| 引用完整性 | 所有 GA/GE/DA 上引用的标签都已注册 | `IsDataValid` + `IDF.Debug.Tags.Validate` |
| 拼写风险 | C++ 侧全部走原生标签常量，禁止裸字符串 `FGameplayTag::RequestGameplayTag("...")` | Code Review 检查项；CI 用 grep 检查（`RequestGameplayTag` 白名单仅限 ini 兜底处） |

---

## 6. GAS 胶水层

### 6.1 `UIDFAbilitySystemComponent`

```cpp
// IDFCore/Public/AbilitySystem/IDFAbilitySystemComponent.h
UCLASS()
class IDFCORE_API UIDFAbilitySystemComponent : public UAbilitySystemComponent
{
    GENERATED_BODY()
public:
    // 输入驱动（Enhanced Input 的 InputTag 直接映射到能力）
    void AbilityInputTagPressed(const FGameplayTag& InputTag);
    void AbilityInputTagReleased(const FGameplayTag& InputTag);

    // 按标签激活（AI 与蓝图都用这个，避免拿 SpecHandle）
    UFUNCTION(BlueprintCallable, Category="IDF|Ability")
    bool TryActivateAbilityByTag(FGameplayTag AbilityTag);

    // 统一失败反馈（第 1 轮 9.5 要求）
    UFUNCTION(BlueprintCallable, Category="IDF|Ability")
    void NotifyAbilityFailed(const FGameplayTag& AbilityTag, const FGameplayTag& FailReason);

    // 属性变化订阅（UI/表现统一走这里，避免各处直接 GetGameplayAttributeValueChangeDelegate）
    FOnGameplayAttributeValueChange& GetIDFAttributeChangeDelegate(const FGameplayAttribute& Attribute);

    // 调试
    void PrintDebugInfo() const;      // 由 IDF.Debug.GAS 1 触发

protected:
    UPROPERTY() TArray<FGameplayAbilitySpecHandle> InputPressedSpecHandles;
    UPROPERTY() TArray<FGameplayAbilitySpecHandle> InputHeldSpecHandles;
    UPROPERTY() TArray<FGameplayAbilitySpecHandle> InputReleasedSpecHandles;
};
```

**输入 → 能力 的映射机制**（避免每个角色写一套输入绑定）：

```
IA_Interact (EnhancedInput) ──[在 InputConfig 里标记 InputTag=Input.Interact]──▶
UIDFAbilitySystemComponent::AbilityInputTagPressed(Input.Interact) ──▶
遍历 FGameplayAbilitySpec：DynamicAbilityTags 或 AbilityTags 含 Input.Interact 的 Spec ──▶ TryActivateAbility(Handle)
```

> 第 3 轮实现时注意：`FGameplayAbilitySpec::GetDynamicSpecSourceTags()` 是推荐做法，`AbilityTags` 在实例化后不再可靠地用于输入匹配。

### 6.2 `UIDFAbilitySet`（一份资产搞定装配）

```cpp
// IDFCore/Public/AbilitySystem/IDFAbilitySet.h
USTRUCT()
struct FIDFAbilitySet_Ability
{
    GENERATED_BODY()
    UPROPERTY(EditDefaultsOnly) TSubclassOf<UGameplayAbility> Ability;
    UPROPERTY(EditDefaultsOnly) int32 Level = 1;
    UPROPERTY(EditDefaultsOnly) FGameplayTag InputTag;      // 空表示非输入型（被动）
};

USTRUCT()
struct FIDFAbilitySet_GameplayEffect
{
    GENERATED_BODY()
    UPROPERTY(EditDefaultsOnly) TSubclassOf<UGameplayEffect> Effect;
    UPROPERTY(EditDefaultsOnly) float Level = 1.f;
};

USTRUCT()
struct FIDFAbilitySet_AttributeSet
{
    GENERATED_BODY()
    UPROPERTY(EditDefaultsOnly) TSubclassOf<UAttributeSet> AttributeSet;
};

UCLASS(BlueprintType)
class IDFCORE_API UIDFAbilitySet : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    // 返回授予的 Handle，供后续移除（重生/换角色）
    void GiveToAbilitySystem(UAbilitySystemComponent* ASC, TArray<FGameplayAbilitySpecHandle>* OutHandles = nullptr) const;

    UPROPERTY(EditDefaultsOnly, Category="Abilities")     TArray<FIDFAbilitySet_Ability>        GrantedAbilities;
    UPROPERTY(EditDefaultsOnly, Category="Effects")       TArray<FIDFAbilitySet_GameplayEffect> GrantedEffects;
    UPROPERTY(EditDefaultsOnly, Category="Attributes")    TArray<FIDFAbilitySet_AttributeSet>   GrantedAttributes;
};
```

### 6.3 `UIDFInputConfig`

```cpp
UCLASS(BlueprintType)
class IDFCORE_API UIDFInputConfig : public UDataAsset
{
    GENERATED_BODY()
public:
    // 原生输入（移动/相机/蹲/冲刺）——不走 GAS
    UPROPERTY(EditDefaultsOnly) TObjectPtr<UInputMappingContext> DefaultMappingContext;
    UPROPERTY(EditDefaultsOnly) TObjectPtr<UInputAction> MoveAction;
    UPROPERTY(EditDefaultsOnly) TObjectPtr<UInputAction> LookAction;
    UPROPERTY(EditDefaultsOnly) TObjectPtr<UInputAction> JumpAction;   // 预留：本作无跳跃，绑定到"翻越"
    UPROPERTY(EditDefaultsOnly) TObjectPtr<UInputAction> CrouchAction;
    UPROPERTY(EditDefaultsOnly) TObjectPtr<UInputAction> SprintAction;

    // 能力输入（走 GAS）
    UPROPERTY(EditDefaultsOnly) TArray<TObjectPtr<UInputAction>> AbilityInputActions;  // 顺序对应 AbilitySet.InputTag
};
```

### 6.4 初始化时序（第 1 轮 9.1 的落地版）

```
【服务器】
AGameMode::PostLogin → AIDMPlayerState 创建 → SpawnDefaultPawnFor → AIDFCharacterBase::PossessedBy
  └─ InitializeAbilitySystem()
       ├─ AbilitySystemComponent->InitAbilityActorInfo(this, this)
       ├─ for each DefaultAttributeSets: AddAttributeSetSubobject
       ├─ ApplyAbilitySet()  → UIDFAbilitySet::GiveToAbilitySystem(ASC)
       ├─ ApplyInitialEffects()（GE_Init_BaseAttributes）
       ├─ AddLooseGameplayTag(State.Ready)
       └─ GameplayMessage 广播 Event.Character.Ready (Payload: FIDFCharacterReadyMessage{Character, Team})

【客户端】
AIDFCharacterBase::OnRep_PlayerState / OnRep_Controller 触发（可能多次）
  └─ if (!bAbilitySystemInitialized) InitializeAbilitySystem()   ← 与服务器同一套代码
       └─ 本地玩家额外：AIDMPlayerController 绑定 InputConfig 与 IMC

【顺序陷阱（必须遵守）】
1. AttributeSet 必须在应用 GE 之前加入（否则 GE 修改找不到属性 → 静默失败）
2. State.Ready 必须在 GiveAbilitySet 之后加（否则能力初始化时的标签查询不一致）
3. UI 订阅 Event.Character.Ready，而不是在 BeginPlay 里抢跑（否则读到未初始化的 ASC）
```

### 6.5 本轮落地的能力（占位，验证链路）

| 能力 | 标签 | 作用 | 验证点 |
|---|---|---|---|
| `GA_IDF_Interact` | `Ability.Survivor.Decode`（临时复用） | 调用 `UIDFInteractionComponent::OnInteractPressed/Released` | 输入 → GAS → 交互链路打通 |
| `GA_IDF_Sprint` | `Ability.Common.Sprint` | 按住冲刺（改 `MaxWalkSpeed`） | 移动属性链路 |
| `GA_IDF_Crouch` | `Ability.Common.Crouch` | 蹲行开关 | 状态标签链路 |

> 真正的角色技能（医生自疗等）在 M5 用同一套 `AbilitySet` 注入，不改框架。

---

## 7. 网络同步：时间基准与复制策略

### 7.1 `UIDFNetClockComponent`（服务器时间唯一真相，ADR-5 落地）

| 项 | 设计 |
|---|---|
| 目的 | 破译进度、椅子计时、大门进度都必须基于**服务器时间**，避免 `DeltaTime` 累加漂移（第 1 轮 R3） |
| 权威时间 | 服务器用 `GetWorld()->GetTimeSeconds()`（GameState 的 `GetServerWorldTimeSeconds()` 亦可），作为 `ServerTime` 复制到客户端 |
| 客户端时钟 | 维护 `ServerTimeOffset = ServerTime - LocalTime`，带 0.1s 平滑；对外暴露 `GetServerTime()` |
| 漂移处理 | 每 1s 由 GameState 广播一次 `ServerTime`（`ReplicatedFloat` + `OnRep`），客户端用加权滑动平均更新偏移 |
| 与延迟的关系 | 不做"预测服务器时间"（5 人局域网/PIE 无必要），只做平滑对齐 |

```cpp
UCLASS(ClassGroup=(IDF), meta=(BlueprintSpawnableComponent))
class IDFCORE_API UIDFNetClockComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure) float GetServerTime() const;
    UFUNCTION(BlueprintPure) float GetServerTimeOffset() const { return ServerTimeOffset; }
protected:
    void OnRep_ServerTime();
    UPROPERTY(ReplicatedUsing=OnRep_ServerTime) float ServerTime = 0.f;
    UPROPERTY() float ServerTimeOffset = 0.f;
};
```

### 7.2 复制字段与频率总表（本轮定稿）

| 类 | 字段 | 复制方式 | 频率/条件 | 备注 |
|---|---|---|---|---|
| `AIDFCharacterBase` | `Team` | `Replicated` | 一次 | 出生即定 |
| `AIDFCharacterBase` | `LifeStateTag` | `ReplicatedUsing` | 状态变化时 | `RepNotify` 驱动动画/UI |
| `AIDFCharacterBase` | 位置/朝向 | `ReplicateMovement`（内建） | 30 Hz | `NetUpdateFrequency=30`, `MinNetUpdateFrequency=10` |
| `AIDFCharacterBase` | 速度乘区 | GAS 属性（`Replicated`） | 变化时 | `MoveSpeedMultiplier` |
| `UIDFInteractionComponent` | `ActiveOptionTag` / `ActiveTarget` | `Replicated` | 交互开始/结束 | 供其他客户端播表现 |
| `UIDFInteractableStateComponent` | `Progress01` | `Replicated` | 0.1s 或变化 >0.5% | `SetNetUpdateFrequency(10)` |
| `UIDFInteractableStateComponent` | `bCompleted` / `StateTag` | `ReplicatedUsing` | 一次性/状态变化 | `RepNotify` 播特效与音效 |
| 交互点 Actor | 静态属性 | **不复制** | — | 关卡内置：位置、`InteractionTag`、`RegionTag` 都是关卡数据，不占带宽 |
| `AIDMGameState` | `MatchPhase`、`DecodedCount`、`ServerTime` | `ReplicatedUsing` | 变化时 / 1s | UI 与规则共用 |
| `AIDMPlayerState` | `Team`、`CharacterTag`、`bReady` | `Replicated` | 变化时 | 选人流程用 |

**相关性与裁剪**：
- 交互点 `AIDM*Actor`：`bAlwaysRelevant = false`，`NetCullDistanceSquared = 15000^2`（150 m）；
- 角色：保持默认相关（5 人图无需过滤）；
- 纯装饰（集装箱、废墟）：`bReplicates = false`，作为关卡静态网格存在。

### 7.3 RPC 清单（本轮）

| RPC | 方向 | 可靠性 | 用途 |
|---|---|---|---|
| `Server_BeginInteraction` | Client → Server | Reliable | 开始交互请求（带 `SequenceId`） |
| `Server_EndInteraction` | Client → Server | Reliable | 结束交互 |
| `Client_InteractionRejected` | Server → Client | Reliable | 拒绝回执，停止本地表现 |
| `Multicast_InteractionStarted/Ended` | Server → All | Unreliable | 表现广播（丢失也能靠状态复制兜底） |
| `Server_RequestCharacterSelect` | Client → Server | Reliable | 本地选人（第 8 章） |
| `Client_CharacterSelectResult` | Server → Client | Reliable | 选人结果/理由 |

**RPC 使用纪律**：
1. `WithValidation` 只用于**参数合法性**（空指针、非法标签、数值越界），业务校验仍走 `CanInteract`；
2. 校验失败**不要**踢人、不要弹窗，只记日志 + 静默丢弃（第 1 轮 4.2 铁律）；
3. 高频动作（移动、相机、攻击蓄力）一律不走 RPC，走复制属性或 CharacterMovement。

---

## 8. `IDMMatch`：规则层与本地流程

### 8.1 `UIDMMatchRuleSet`（数值唯一入口，ADR-6 落地）

```cpp
UCLASS(BlueprintType, Const)
class IDMMATCH_API UIDMMatchRuleSet : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    // ── 队伍（D2：固定 4v1，仅作常量校验）──
    UPROPERTY(EditDefaultsOnly, Category="Team") int32 SurvivorCount = 4;
    UPROPERTY(EditDefaultsOnly, Category="Team") int32 HunterCount   = 1;

    // ── 密码机 ──
    UPROPERTY(EditDefaultsOnly, Category="Machine") int32 TotalMachines     = 7;   // [还原]
    UPROPERTY(EditDefaultsOnly, Category="Machine") int32 RequiredMachines  = 5;   // [还原]
    UPROPERTY(EditDefaultsOnly, Category="Machine") float DecodeDurationOne  = 75.f; // 单人满破译秒数 [待标定]
    UPROPERTY(EditDefaultsOnly, Category="Machine") float CalibrationIntervalMin = 6.f;
    UPROPERTY(EditDefaultsOnly, Category="Machine") float CalibrationIntervalMax = 10.f;
    UPROPERTY(EditDefaultsOnly, Category="Machine") float CalibrationFailPenalty = 0.08f;
    UPROPERTY(EditDefaultsOnly, Category="Machine") float CalibrationFailRevealTime = 5.f;

    // ── 大门 ──
    UPROPERTY(EditDefaultsOnly, Category="Gate") int32 GateCount = 2;
    UPROPERTY(EditDefaultsOnly, Category="Gate") float GateOpenDuration = 20.f;   // 单人满开门秒数 [待标定]

    // ── 狂欢之椅（D4：挣扎简化版）──
    UPROPERTY(EditDefaultsOnly, Category="Chair") float ChairPhase1Duration = 30.f;  // [还原音译]
    UPROPERTY(EditDefaultsOnly, Category="Chair") float ChairPhase2Duration = 25.f;
    UPROPERTY(EditDefaultsOnly, Category="Chair") float ChairBasementBonus  = 10.f;
    UPROPERTY(EditDefaultsOnly, Category="Chair") float RescueDuration      = 1.0f;  // [还原]
    UPROPERTY(EditDefaultsOnly, Category="Chair") float StruggleTimeExtend  = 2.0f;  // 简化挣扎：每次 +2s

    // ── 地窖 ──
    UPROPERTY(EditDefaultsOnly, Category="Hatch") int32 HatchRequiredMachines = 2;
    UPROPERTY(EditDefaultsOnly, Category="Hatch") int32 HatchSurvivorsLeft    = 1;

    // ── 对局 ──
    UPROPERTY(EditDefaultsOnly, Category="Match") float WarmupDuration = 15.f;
    UPROPERTY(EditDefaultsOnly, Category="Match") float MatchTimeLimit = 0.f;   // 0 = 不限时
    UPROPERTY(EditDefaultsOnly, Category="Match") bool  bAutoStartInEditor = true; // PIE 测试直开

    // 数据校验（DataValidation 插件）
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
};
```

### 8.2 `AIDMGameState`

```cpp
UENUM(BlueprintType)
enum class EIDMMatchPhase : uint8 { Lobby, Loading, Warmup, Playing, PostMatch };

UCLASS()
class IDMMATCH_API AIDMGameState : public AGameStateBase
{
    GENERATED_BODY()
public:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void Tick(float DeltaSeconds) override;

    UFUNCTION(BlueprintPure) EIDMMatchPhase GetMatchPhase() const { return MatchPhase; }
    UFUNCTION(BlueprintPure) int32 GetDecodedCount() const { return DecodedCount; }
    UFUNCTION(BlueprintPure) int32 GetRequiredMachines() const;
    UFUNCTION(BlueprintPure) float GetServerTimeSeconds() const;

    // 仅服务器
    void SetMatchPhase(EIDMMatchPhase NewPhase);
    void NotifyMachineDecoded(AIDMMachineRegistryComponent* /*M4*/);

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchPhaseChanged, EIDMMatchPhase, NewPhase);
    UPROPERTY(BlueprintAssignable) FOnMatchPhaseChanged OnMatchPhaseChanged;

protected:
    UPROPERTY(ReplicatedUsing=OnRep_MatchPhase) EIDMMatchPhase MatchPhase = EIDMMatchPhase::Lobby;
    UPROPERTY(ReplicatedUsing=OnRep_DecodedCount) int32 DecodedCount = 0;
    UPROPERTY(ReplicatedUsing=OnRep_ServerTime) float ServerTime = 0.f;
    UPROPERTY() TObjectPtr<UIDMMatchRuleSet> RuleSet;

    UFUNCTION() void OnRep_MatchPhase();
    UFUNCTION() void OnRep_DecodedCount();
    UFUNCTION() void OnRep_ServerTime();
    float TimeSinceLastClockSync = 0.f;
};
```

**`ServerTime` 的复制频率**：1 Hz（`Tick` 内累计 1s 才更新），足够破译/椅子计时对齐，不占带宽。

### 8.3 `AIDMPlayerState`

```cpp
UCLASS()
class IDMMATCH_API AIDMPlayerState : public APlayerState, public IAbilitySystemInterface
{
    GENERATED_BODY()
public:
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;   // 返回 Character 的 ASC（本轮）

    UFUNCTION(BlueprintPure) EIDFTeam GetTeam() const { return Team; }
    UFUNCTION(BlueprintPure) FGameplayTag GetCharacterTag() const { return CharacterTag; }
    UFUNCTION(BlueprintPure) bool IsReady() const { return bReady; }

    void SetTeam(EIDFTeam InTeam);                 // 仅服务器
    void SetCharacterTag(FGameplayTag InTag);      // 仅服务器
    void SetReady(bool bInReady);                  // 仅服务器
protected:
    UPROPERTY(Replicated) EIDFTeam Team = EIDFTeam::None;
    UPROPERTY(Replicated) FGameplayTag CharacterTag;
    UPROPERTY(Replicated) bool bReady = false;
    UPROPERTY(Replicated) bool bDisconnected = false;   // 第 1 轮 4.5 预留
};
```

### 8.4 本地流程（D6：不做真联机，简化为"直进对局"）

```
[PIE 启动 5 个窗口]
  ├─ 窗口 1（Server + Client）：监听服，PostLogin 依次接纳 4 个客户端
  ├─ 每个窗口：无需登录/无需匹配
  ├─ 进入 L_MilitaryFactory
  ├─ Warmup 阶段（可在 RuleSet 里设为 0 秒以加速测试）
  │    ├─ 服务器按 PlayerState 的索引分配阵营：索引 0 → 监管者，1..4 → 求生者
  │    └─ 每个客户端可在占位选人 UI（一个纯 UMG 列表）里改角色；本轮不真正加载角色插件
  └─ Playing 阶段：本轮只验证移动 + 交互提示，不判胜负
```

**为什么保留一个"选人 UI"**：它是 M5 验证 GameFeature 动态加载的载体（`UGameFeatureSubsystem::LoadAndActivateGameFeaturePlugin`）。本轮先做 UI 与 RPC 链路（`Server_RequestCharacterSelect`），加载留到 M5。

### 8.5 `AIDMGameMode` / `AIDMPlayerController`

| 类 | 职责（本轮） |
|---|---|
| `AIDMGameMode` | 读取 `UIDMMatchRuleSet`（从 `AIDMWorldSettings` 或默认值）；`PostLogin` 分配阵营与 `PlayerStart`；驱动 `MatchPhase`；`bUseSeamlessTravel=false`；对局结束（本轮不触发） |
| `AIDMPlayerController` | 绑定 `UIDFInputConfig`；把 `IA_*` 路由给角色（移动）与 ASC（能力）；创建 `UIDFPrimaryGameLayout`；`Server_RequestCharacterSelect` 的实现方 |
| `AIDMWorldSettings` | 地图级默认 `RuleSet`、地图级 `RegionTag` 命名空间（M3 用） |

---

## 9. 本地 5 窗口测试环境（D6 落地）

### 9.1 目标拓扑

```
┌──────────────────────────────────────────────────────────────┐
│ UE 编辑器（单进程，PIE）                                       │
│                                                              │
│  ┌────────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐
│  │ Window 1   │  │Window 2 │  │Window 3 │  │Window 4 │  │Window 5 │
│  │ Listen     │  │ Client  │  │ Client  │  │ Client  │  │ Client  │
│  │ Server +   │  │         │  │         │  │         │  │         │
│  │ Client     │  │         │  │         │  │         │  │         │
│  └────────────┘  └─────────┘  └─────────┘  └─────────┘  └─────────┘
└──────────────────────────────────────────────────────────────┘
```

### 9.2 配置（`Config/DefaultEditorPerProjectUserSettings.ini`）

```ini
[/Script/UnrealEd.LevelEditorPlaySettings]
PlayNetMode=PIE_ListenServer
RunUnderOneProcess=False
PlayNumberOfClients=5
PlayNetDedicated=False
bLaunchSeparateServer=False
; 窗口布局：5 个窗口平铺，避免重叠
CenterConsoleCommand=1
; 网络模拟（可选，验收时打开）
bHasNetworkEmulationSettings=True
NetworkEmulationProfileName=Average
; 让客户端能连本机监听服
bAllowNonStandardPort=false
; 尺寸
NewWindowWidth=960
NewWindowHeight=540
```

### 9.3 多进程 PIE 的注意点（必须预先知道，否则会卡住）

| 问题 | 现象 | 对策 |
|---|---|---|
| 端口冲突 | 5 个进程抢 7777 | `bAllowNonStandardPort`/`RunUnderOneProcess=True` 二选一。**首选 `RunUnderOneProcess=False` + UE 自动分配非标准端口**；若你的机器上仍冲突，退回 `RunUnderOneProcess=True`（单进程多窗口，网络仍然走真实 UDP 复制） |
| 日志混乱 | 5 个进程日志混在一个窗口 | 用 `-log` 启动编辑器，或看 `Saved/Logs/` 下每个进程独立日志；调试时用 `UE_LOG(LogIDF..., Warning)` 保证默认可见 |
| 焦点/输入 | 只有焦点窗口接收输入 | 属正常；测试时点一下目标窗口再操作 |
| 性能 | 5 个 PIE 窗口同时渲染很吃 GPU | 验收时把 PIE 分辨率降到 720p、关 Lumen 用 `r.DynamicGlobalIlluminationMethod=0`（临时）；或 `PlayNetDedicated` 思路：1 个专用服 + 4 窗口（本轮先不做） |
| 断点调试 | 断点会停住所有进程？ | 不会，但会阻塞该进程；服务器断点会导致客户端等待（`net.PktLag` 类现象）。调试网络问题时优先用日志与 `GameplayInsights` |
| 热重载 | Live Coding 在 PIE 中编译会导致所有窗口断开 | **先停止 PIE 再 Live Coding** |

### 9.4 一键验收脚本（编辑器控制台命令）

| 命令 | 作用 | 本轮是否实现 |
|---|---|---|
| `IDF.Debug.Interact 1` | 屏幕显示所有交互点状态与最近交互提示 | 是 |
| `IDF.Debug.GAS 1` | 打印 ASC 能力/标签/属性 | 是 |
| `IDF.Debug.Tags.Validate` | 标签规范校验 | 是 |
| `IDF.Debug.Teleport Survivor 1` | 把 1 号求生者传送到指定区域（配合 M3 地图） | 是（简易版） |
| `IDF.Match.ForcePhase Playing` | 跳过 Warmup | 是 |

---

## 10. UI（本轮最小集）

### 10.1 页面与控件

| 资产 | 类型 | 内容 | 数据源 |
|---|---|---|---|
| `WBP_IDFPrimaryGameLayout` | `UPrimaryGameLayout` 派生 | Game/Menu/Modal 三层栈 | — |
| `WBP_HUD_Debug` | `UCommonUserWidget` | 队伍、健康占位、剩余密码机占位、服务器时间 | `AIDMGameState` / `AIDMPlayerState` |
| `WBP_InteractionPrompt` | `UCommonUserWidget` | 图标 + "按住 F 破译" + 长按进度环 + 灰显原因 | `UIDFInteractionComponent`（ViewModel） |
| `WBP_CharacterSelect` | `UCommonUserWidget` | 5 个角色的临时选择列表（6 个按钮，占位图标） | `AIDMPlayerState::bReady` |
| `WBP_DebugPanel` | `UCommonUserWidget` | 调试开关按钮（`IDF.Debug.*`） | `UIDFDeveloperSettings` |

### 10.2 数据流（第 1 轮 11.3 的首次落地）

```
AIDMGameState (ReplicatedUsing) ──▶ UIDFHUDViewModel (UObject, FieldNotify)
UIDFInteractionComponent (Delegate) ──▶ UIDFInteractionPromptViewModel
IDFAbilitySystemComponent (AttributeChange Delegate) ──▶ UIDFSkillBarViewModel（本轮仅占位）
        ▲
        └── UMG View (UCommonUserWidget) 通过 MVVM 绑定，只读
```

**本轮纪律**：UI 里**不出现**任何 `GetWorld()->GetAuthGameMode()` 或写操作；所有写操作经 `AIDMPlayerController`。

---

## 11. 输入设计

### 11.1 输入映射（D8：键鼠 + 手柄，不做触屏）

| 动作 | 键鼠 | 手柄 | 类型 | 走 GAS？ |
|---|---|---|---|---|
| 移动 | WASD | 左摇杆 | `IA_Move` (Axis2D) | 否 |
| 视角 | 鼠标 | 右摇杆 | `IA_Look` (Axis2D) | 否 |
| 冲刺 | Left Shift（按住） | LT | `IA_Sprint` (Digital) | 是（`Input.Sprint`） |
| 蹲行 | Left Ctrl（按住） | B | `IA_Crouch` (Digital) | 是（`Input.Crouch`） |
| 交互（单击） | F | X | `IA_Interact` | 是（`Input.Interact`） |
| 交互（长按/持续） | F（按住） | X（按住） | `IA_InteractHold` | 是（`Input.InteractHold`） |
| 技能 1 | Q | RB | `IA_Skill1` | 是（`Input.Skill1`） |
| 技能 2 | E | LB | `IA_Skill2` | 是（`Input.Skill2`） |
| 使用道具 | 1 / 鼠标左键 | Y | `IA_UseItem` | 是（`Input.Item.Use`） |
| 菜单 | Esc | Start | `IA_Menu` | 否（UI 层） |

### 11.2 输入模式切换（CommonUI 负责）

| 场景 | 输入模式 | 说明 |
|---|---|---|
| 对局中 | `GameAndUI`（UI 只吃交互提示，不吃鼠标） | 相机由鼠标控制 |
| 选人界面 | `UIOnly` | 鼠标可选，角色不可动 |
| 结算 | `UIOnly` | 同上 |

> 关键：**选人与对局的输入模式切换由 CommonUI 的 `UCommonActivatableWidget` 栈自动处理**（激活/停用时切模式），不要手写 `SetInputMode`。

---

## 12. 数据资产（本轮定义，M4 起填充）

### 12.1 资产清单

| 资产 | 类型 | 本轮内容 |
|---|---|---|
| `DA_IDF_MatchRuleSet_Default` | `UIDMMatchRuleSet` | 全字段默认值（第 8.1 节数值） |
| `DA_IDF_InputConfig_Survivor` | `UIDFInputConfig` | 求生者默认输入映射 |
| `DA_IDF_InputConfig_Hunter` | `UIDFInputConfig` | 监管者默认输入映射（第三人称，参数不同） |
| `DA_IDF_AbilitySet_Placeholder` | `UIDFAbilitySet` | 三个占位能力（6.5 节） |
| `DT_IDF_MatchTuning` | DataTable | 手感覆盖表（D9 的微调入口） |
| `DA_IDF_DeveloperSettings` | `UIDFDeveloperSettings`(DeveloperSettings) | 调试开关、默认日志级别 |

### 12.2 `UIDFDeveloperSettings`

```cpp
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="IDF 项目设置"))
class IDFCORE_API UIDFDeveloperSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere, Category="Debug") bool  bDrawInteractionDebug = false;
    UPROPERTY(Config, EditAnywhere, Category="Debug") bool  bDrawAbilityDebug     = false;
    UPROPERTY(Config, EditAnywhere, Category="Debug") bool  bVerboseNetLog        = false;
    UPROPERTY(Config, EditAnywhere, Category="Test")  bool  bAutoStartMatchInPIE  = true;
    UPROPERTY(Config, EditAnywhere, Category="Test")  int32 AutoAssignHunterIndex = 0;
};
```

### 12.3 成长属性占位（D5：人格不做，但接口先留）

```cpp
// 第 3 轮人格天赋接入点。本轮只声明、不使用，保证接入时不改框架。
UCLASS()
class IDFCORE_API UIDFGrowthSet : public UAttributeSet
{
    GENERATED_BODY()
public:
    // 人格天赋影响的三类成长属性（乘区）
    UPROPERTY(BlueprintReadOnly, Category="Growth") FGameplayAttributeData DecodeSpeedGrowth;   // 默认 1.0
    UPROPERTY(BlueprintReadOnly, Category="Growth") FGameplayAttributeData HealSpeedGrowth;     // 默认 1.0
    UPROPERTY(BlueprintReadOnly, Category="Growth") FGameplayAttributeData MoveSpeedGrowth;     // 默认 1.0
    // ATTRIBUTE_ACCESSORS 宏 + GetLifetimeReplicatedProps
};
```

**接入规则（写进代码规范）**：所有"角色基础值 × 成长值 × 状态增益"的计算统一走
`FinalValue = BaseAttribute * GrowthAttribute * StateMultiplier`，第 3 轮只需给 `GrowthAttribute` 写入人格加成，不动任何能力代码。

### 12.4 数值落库与微调机制（D9）

| 层 | 载体 | 优先级 | 用途 |
|---|---|---|---|
| 1. 还原基线 | `UIDMMatchRuleSet` / `DA_Character_*` 默认值 | 最低 | 标 `[还原]` 的原作观感值 |
| 2. 微调覆盖 | `DT_IDF_MatchTuning`（DataTable） | 中 | 标 `[待标定]` 的手感值，可在编辑器里改后立刻生效 |
| 3. 运行时调试 | `IDF.Tuning.Set <Key> <Value>` 控制台命令 | 最高（不持久） | 单次对局试参数 |

**读取顺序**：代码只读 `UIDFTuningSubsystem::GetValue(Key)`，它内部按 3→2→1 查找。**禁止**在 GA/GE 里直接读取 RuleSet 的裸字段。

---

## 13. 移动与相机（第三人称，D3 落地）

### 13.1 移动参数（本轮用默认值，M5 起由 GAS 驱动）

| 参数 | 值 | 备注 |
|---|---|---|
| `MaxWalkSpeed` | 400 cm/s（4.0 m/s） | `[待标定]` |
| `MaxWalkSpeedCrouched` | 200 cm/s | `[待标定]` |
| `MaxAcceleration` | 2048 cm/s² | 偏"跟手" |
| `BrakingDecelerationWalking` | 2048 cm/s² | 急停手感 |
| `GroundFriction` | 8.0 | 默认 |
| `bOrientRotationToMovement` | true（求生者）/ true（监管者） | 均第三人称 |
| `bUseControllerRotationYaw` | false | 交给 `bOrientRotationToMovement` |
| `RotationRate` | Yaw 540°/s | `[待标定]` |
| `JumpZVelocity` | 0（禁用跳跃） | 原作无跳跃 |
| `AirControl` | 0.05 | 几乎不可空控 |

### 13.2 相机（求生者与监管者同一框架，参数不同）

| 参数 | 求生者 | 监管者 | 说明 |
|---|---|---|---|
| 相机类型 | `USpringArmComponent` + `UCameraComponent` | 同 | 第 3 轮可换 `GameplayCameras` 的 Camera Rig |
| 臂长 | 350 cm | 420 cm | `[待标定]`；监管者视野更"压" |
| 臂高 | 80 cm | 110 cm | — |
| FOV | 90 | 85 | `[待标定]` |
| 碰撞检测 | 开启（探头防穿墙） | 开启 | `bDoCollisionTest=true`，`ProbeChannel=ECC_Camera` |
| 相机延迟 | 移动 0.3s / 旋转 0.15s | 移动 0.2s / 旋转 0.1s | 用 `CameraLagSpeed` |
| 受击摇晃 | GameplayCue 驱动的相机抖动 | 同 | M5 接入 |

> **D3 的影响**：取消了 FPP 手臂骨骼、FPP 专用动画与"第一人称武器"逻辑，M2 的动画工作量减少约 30%；但监管者的"压迫感"需要通过更近的臂长 + 更强的相机延迟 + 音效来补。

---

## 14. 本轮验收标准（AC-R2）

| 编号 | 验收项 | 通过条件 | 验证方式 |
|---|---|---|---|
| AC-R2-1 | 工程可编译 | 编辑器打开无报错，`Identity5` + 2 个 GameFeature 插件编译通过 | 手动 |
| AC-R2-2 | 5 窗口联机 | 5 个 PIE 窗口全部进入游戏，各自生成 1 个角色 | 手动 |
| AC-R2-3 | 移动同步 | 任一窗口移动，其余 4 窗口在 < 200ms 内看到位置更新 | 手动 + `stat net` |
| AC-R2-4 | 阵营分配 | 1 个监管者 + 4 个求生者，`Team` 在所有窗口一致 | 屏幕调试信息 |
| AC-R2-5 | GAS 生效 | 每个角色 `State.Ready` 标签存在，占位能力可被输入激活（调试打印） | `IDF.Debug.GAS 1` |
| AC-R2-6 | 交互提示 | 靠近假密码机出现"按住 F 破译"，离开消失，灰显原因正确 | 手动 |
| AC-R2-7 | 服务器裁定 | 客户端强制发起超距离交互请求（调试命令），服务器拒绝且客户端表现停止 | `IDF.Debug.Interact 1` + 日志 |
| AC-R2-8 | 进度同步 | 破译进度在 5 端一致（误差 < 1%） | 屏幕数值对比 |
| AC-R2-9 | 标签校验 | `IDF.Debug.Tags.Validate` 无报错 | 控制台 |
| AC-R2-10 | 插件清单正确 | 按 `Documents/plugin_setup.md` 开启后无缺失模块报错 | 手动 |

---

## 15. 第 3 轮预告（实现轮）

按 D1~D9 与本轮骨架，第 3 轮进入**写代码**，顺序如下：

1. **Week A**：`Identity5.Build.cs` + `IDFCore` / `IDMMatch` 插件骨架落地；`IDFGameplayTags` 全量声明；`UIDFAbilitySystemComponent` / `UIDFAbilitySet` / `UIDFInputConfig`；`AIDFCharacterBase` 的 ASC 初始化；`AIDMGameState` / `AIDMPlayerState` / `AIDMGameMode` / `AIDMPlayerController`；5 窗口 PIE 跑通（AC-R2-1~AC-R2-5）。
2. **Week B**：`IIDFInteractableTarget` + `UIDFInteractionComponent` + `UIDFInteractableStateComponent` + 假密码机 + 交互提示 UI + 服务器校验（AC-R2-6~AC-R2-9）。
3. **Week C**：移动手感（第三人称相机参数、`CharacterMovement` 调参）、`AIDFDebugMachineActor` 状态机资产化（StateTree）、`UIDFNetClockComponent` 与服务器时间对齐、`Documents/testing_pie.md` / `plugin_setup.md` 成文，然后进入 M2（翻越框架 + 动画）。

> **下一步需要你做的只有一件事**：按第 2.1 节 A 表在编辑器中开启 11 个插件并重启编辑器。完成后告诉我，我就可以开始第 3 轮（真正写代码）。若你希望我先只落 `Build.cs` + 标签 + 插件骨架（不等你开插件），也可以直接让我开始——但那时编译会因缺少 GAS 模块而失败，需要等你开完再验证。

