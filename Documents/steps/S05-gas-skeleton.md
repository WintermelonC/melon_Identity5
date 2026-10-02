# S05 实施步骤：最小 GAS 骨架（ASC + AttributeSet）

> 对应 `implementation_roadmap.md` 的 S05。**目标**：在 **`IDFCore` GameFeature 插件**里实现 `UIDFAbilitySystemComponent` 与 `UIDFHealthSet`，编译通过、宏与反射都验证无误、属性在客户端能正确复制。
> 本节预计 5~7 小时（比原估多 1 小时，因为要先建插件）。**这是整个项目技术含量最高部分的开端，也是坑最密集的一节**——GAS 的报错往往是"静默失败"（属性不复制、修改不生效），所以我给你逐行解释 + 验证手段。

---

## -1. 前置动作：创建 `IDFCore` GameFeature 插件（必须先做）

### -1.1 为什么 GAS 骨架必须住在插件里，而不是主模块

`design_analysis.md` 第 2 轮 2.1 节已经把 `IDFCore` 定义为「**核心框架：交互抽象、角色基类、GAS 胶水、消息、复制组件**」。本节要建的自定义 ASC 与 AttributeSet，正是"GAS 胶水"。

**这不是洁癖，而是三个实际后果**：

| 项 | 放主模块 | 放 `IDFCore` 插件 |
|---|---|---|
| 依赖方向 | 角色插件要 include 主模块 → **反向依赖**，破坏设计文档 2.1 节的单向依赖图 | 主模块与角色插件都依赖 `IDFCore` → **符合设计** |
| `Public/` 的封装含义 | 无意义（单模块里 `Public/Private` 只是形式） | 有意义：`Private/` 对外真正不可见 |
| S20 的验收（禁用任一插件其余正常） | 做不到——GAS 是所有人的地基，却和装配代码混在一起 | 能做到——`IDFCore` 是独立可装卸单元 |

**为什么"以后再迁移"代价很高**（这是必须现在决定的原因）：C++ 类**换模块不是移动文件**。一旦 S06 开始创建引用这些类的资产（`DA_IDF_AbilitySet`、`GE_*`、蓝图父类），跨模块搬迁会导致这些资产断链，需要逐个人工重定向，还常留下 `REINST_` 孤儿类。**而现在（S05）你只有 4 个源文件、零资产引用，是零代价窗口。**

### -1.2 用编辑器创建插件

```
编辑器 → 编辑（Edit）→ 插件（Plugins）→ 右上角 "Add"（添加）
  → 选择 Game Feature 模板（名称类似 "Game Feature (with C++ support)" / Game Feature）
  → Name: IDFCore
  → 位置：项目的 Plugins/GameFeatures/ 目录下
  → Create Plugin → 提示重启编辑器时选 "Restart Now"
```

⚠️ **如果模板列表里没有 Game Feature 类型**（不同引擎版本/安装方式会影响模板可见性），退而求其次：选一个**空白 C++ 插件**创建，然后手动把 `.uplugin` 改成下面的内容。

### -1.3 核对 `.uplugin`

创建后打开 `Plugins/GameFeatures/IDFCore/IDFCore.uplugin`，确认关键字段（这是我的推荐版本，可直接对照/替换）：

```json
{
	"FileVersion": 3,
	"Version": 1,
	"VersionName": "1.0",
	"FriendlyName": "IDF Core",
	"Description": "Identity5 核心框架层：交互抽象、GAS 胶水、角色基类、消息与复制组件",
	"Category": "Gameplay",
	"CreatedBy": "WintermelonC",
	"CanContainContent": true,
	"ExplicitlyLoaded": false,
	"Modules": [
		{
			"Name": "IDFCore",
			"Type": "Runtime",
			"LoadingPhase": "Default"
		}
	],
	"Plugins": [
		{ "Name": "GameplayAbilities", "Enabled": true },
		{ "Name": "GameFeatures", "Enabled": true },
		{ "Name": "ModularGameplay", "Enabled": true }
	]
}
```

> ⚠️ **`"Plugins"` 里只能写"真实存在的插件"**——引擎会去插件注册表里查找这些名字，找不到就**直接构建失败**：
> ```
> Unable to find plugin 'GameplayTags' (referenced via Identity5.uproject -> IDFCore.uplugin).
> Install it and try again, or remove it from the required plugin list.
> ```
> `GameplayTags` 是**引擎内置模块**（`Engine/Source/Runtime/GameplayTags/`，**没有 `.uplugin` 文件**），所以：
> - ❌ 不能写在 `.uplugin` 的 `"Plugins"` 数组里；
> - ✅ 要写在 `Build.cs` 的 `DependencyModuleNames` 里（见 1.1 节）。
>
> **两处区别一句话记住**：`.uplugin` 的 `"Plugins"` = **插件**依赖；`Build.cs` 的 `DependencyModuleNames` = **模块**依赖。`GameplayAbilities` 两处都出现是正常的（它既是插件也有同名模块）；`GameplayTags` 只在 `Build.cs` 里。

**四个关键字段的作用**：

| 字段 | 作用 | 写错的后果 |
|---|---|---|
| `"Type": "Runtime"`（在 `Modules[]` 里） | **模块**类型：`Runtime` 表示"游戏运行时也要加载" | 这是**模块级**字段，必须留着。取值还有 `Editor`（仅编辑器）/`Developer`/`UncookedOnly` |
| `"CanContainContent": true` | 允许插件里有 `Content/` 目录 | S06 创建 `DA_*`/`GE_*` 资产时**无处可放** |
| `"ExplicitlyLoaded": false` | 是否"等别人请求才挂载"：`false` = 随进程启动挂载 | 若为 `true`，插件要由 `UGameFeaturesSubsystem` 显式请求才挂载，框架层插件的类可能晚于主模块注册 |
| `"LoadingPhase": "Default"` | **模块**加载时机 | 不要用 `PreDefault`：那会早于 `Engine` 模块的反射注册，可能出现"找不到类" |
| `"BuiltInInitialFeatureState": "Active"` | GameFeature 启动时推进到的状态（合法值 `Installed`/`Registered`/`Loaded`/`Active`） | 非 GameFeature 插件会忽略它；写错值引擎会在日志里报 `unknown value` 并退回 `Active` |

> ⚠️ **一个重要澄清**：**插件描述符里没有 `"Type"` 字段**。我核实过 `PluginDescriptor.h` 的全部字段（`FPluginDescriptor`），没有 `Type`；`PluginDescriptor.cpp` 读取的 JSON 键里也没有 `Type`——只有 `ModuleDescriptor.cpp:174` 在读 `TEXT("Type")`。
>
> 也就是说，`"Type"` **只出现在 `Modules[]` 的每个条目里**（模块类型）。而"插件的类型"是由**位置**推断的（枚举 `EPluginType`：`Engine` / `Enterprise` / `Project` / `External` / `Mod`）——你的插件在 `项目/Plugins/GameFeatures/` 下，所以类型是 `Project`。
>
> 那"GameFeature 身份"从哪来？两样东西：
> 1. **`Content/IDFCore.uasset`**（`UGameFeatureData` 数据资产）——引擎靠它识别这是 GameFeature；
> 2. **`BuiltInInitialFeatureState`**——由 `UGameFeatureSubsystem::DetermineBuiltInInitialFeatureState()` 读取（`GameFeaturesSubsystem.cpp:5045`）。

**`"Plugins"` 数组里能写什么**（这是个高频踩坑点）：

数组里只能写**真实存在的插件名**（必须有对应的 `.uplugin` 文件）。引擎会去插件注册表查找，找不到就**直接构建失败**，报错形如：

```
Unable to find plugin 'GameplayTags' (referenced via Identity5.uproject -> IDFCore.uplugin).
Install it and try again, or remove it from the required plugin list.
```

本项目的正确写法是那 3 个（`GameplayAbilities` / `GameFeatures` / `ModularGameplay`）。**`GameplayTags` 不能写在这里**——它是引擎内置模块（`Engine/Source/Runtime/GameplayTags/`，没有 `.uplugin`），正确位置是 `Build.cs` 的 `DependencyModuleNames`。

**两处区别一句话记住**：

| 写在哪 | 表达什么 | 合法取值 | 例子 |
|---|---|---|---|
| `.uplugin` 的 `"Plugins"` | **插件**依赖 | 必须是有 `.uplugin` 的插件 | `GameplayAbilities`、`CommonUI` |
| `Build.cs` 的 `DependencyModuleNames` | **模块**依赖 | 任何模块名（含引擎模块） | `GameplayTags`、`Engine`、`Core` |

`GameplayAbilities` 两处都出现是正常的（它既是插件、又有同名模块）；`GameplayTags` 只在 `Build.cs` 里。

### -1.4 确认插件已在 `.uproject` 注册

打开 `Identity5.uproject`，`Plugins` 数组里应该有：

```json
{
	"Name": "IDFCore",
	"Enabled": true
}
```

**如果编辑器没自动加，手动加**。漏了的后果是：UBT 不编译该插件模块 → 链接时 `unresolved external symbol ... UIDFHealthSet::StaticClass()`。

### -1.5 修正 API 宏（本节最容易踩的坑）

插件的类**不能再用 `IDENTITY5_API`**：UBT 按**模块名**生成导出宏，`IDFCore` 模块对应的是 `IDFCORE_API`（模块名大写 + `_API`）。

```cpp
// ❌ 在 IDFCore 插件里错误：沿用了主模块的宏
class IDENTITY5_API UIDFHealthSet : public UAttributeSet

// ✅ 正确：模块名 IDFCore → UBT 生成的宏是 IDFCORE_API
class IDFCORE_API UIDFHealthSet : public UAttributeSet
```

**漏改的症状很有迷惑性**：编译能过，但**链接失败**（`unresolved external symbol ...UIDFHealthSet::StaticClass()`），或运行时"找不到类"。原因是 `IDENTITY5_API` 把符号导出到主模块，而类实际定义在 `IDFCore` 模块里。

> 反过来说：**如果你是从零新建文件放进插件**（本节的正常做法），只要一开始就写 `IDFCORE_API`，根本不会遇到这个问题。这条坑主要针对"把已有代码从主模块搬进插件"的场景。

### -1.6 关于日志类别：顺便把归属定对

`design_analysis.md` 第 2 轮约定「**平台类日志归主模块，系统类日志归所属插件**」。本节一起落地（**注意完整路径里有 `Source/IDFCore/`**）：

```
Plugins/GameFeatures/IDFCore/Source/IDFCore/Public/Logging/IDFCoreLog.h    ← 新增：LogIDFNet / LogIDFAbility / LogIDFAttribute / LogIDFInteraction
Plugins/GameFeatures/IDFCore/Source/IDFCore/Private/Logging/IDFCoreLog.cpp ← 新增
Source/Identity5/Public/Logging/Identity5Log.h                             ← 修改：只保留 LogIdentity5
Source/Identity5/Private/Logging/Identity5Log.cpp                          ← 修改：同步删除
```

`LogIDFMatch` 到 S10 建 `IDMMatch` 插件时再迁过去；`LogIDFAI` / `LogIDFUI` 到对应里程碑再说。

⚠️ **迁移的连带改动**（逐条核对，漏一条就编译失败）：

| # | 改动 | 说明 |
|---|---|---|
| 1 | `Identity5Log.h` 删除这 4 行声明 | `LogIDFNet` / `LogIDFAbility` / `LogIDFAttribute` / `LogIDFInteraction` |
| 2 | `Identity5Log.cpp` 删除对应的 4 行 `DEFINE_LOG_CATEGORY` | **必须与第 1 条同时做**，否则链接报 `duplicate symbol` 或 `unresolved external symbol` |
| 3 | 检查 `Identity5DebugCommands.cpp` | 它当前引用的是 `LogIdentity5`（**主模块自己的类别**），所以**不需要改动**。若将来它要打 `LogIDFInteraction`，主模块已依赖 `IDFCore`（见 1.2 节），可直接用 |
| 4 | `Config/DefaultEngine.ini` 的 `[Core.Log]` 段 | **不用改**——类别名没变，只是定义位置从主模块移到了插件 |
| 5 | 新文件 `IDFCoreLog.h/.cpp` | 内容就是原来 `Identity5Log` 里那 4 个类别的声明与定义，照搬即可 |

> **落地的两个选择**：
> - **A（推荐，本节的顺序）**：先建 `IDFCoreLog.h/.cpp` 并清空主模块的 4 个类别，然后写 GAS 代码（GAS 代码里用的 `LogIDFAttribute` / `LogIDFAbility` 直接来自 `IDFCoreLog.h`）；
> - **B（更稳，但分两次提交）**：先只把 GAS 文件写进插件、并暂时 include 主模块的 `Identity5Log.h`（此时插件反向依赖主模块）。
>
> **选 A**。B 会临时制造"插件依赖主模块"的反向依赖，虽然能跑，但违反设计文档的依赖图，还要多改一次代码。


---

## 0. 本节要建立的四个正确认知（先读，能省你半天）

### 认知 1：`PreAttributeChange` 与 `PostGameplayEffectExecute` 分工完全不同

这是 GAS 里最容易被混用的一对回调。引擎注释写得很清楚（`AttributeSet.h:196-219`）：

| 回调 | 触发时机 | 该做什么 | **不该做什么** |
|---|---|---|---|
| `PreAttributeChange(Attribute, float& NewValue)` | **任何**修改（含持续 GE 生效、GE 移除、堆叠变化）都会调用 | 单纯**夹取最终值**（`Health = Clamp(Health, 0, MaxHealth)`） | ❌ 不要在这里触发游戏事件（死亡、受击特效）——它调用极其频繁，且 `NewValue` 只是"即将生效的当前值"，不是 BaseValue |
| `PreAttributeBaseChange(Attribute, float& NewValue) const` | 仅当**基础值**被修改（GE 执行） | 夹取基础值（和上面的夹取保持一致的规则） | 同上 |
| `PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)` | GE **执行**后（Instant GE 生效） | 结算逻辑：读取 Meta 属性 → 换算伤害 → 改 Health → **清空 Meta** → 广播事件 | ❌ 不要以为它每帧调用（只在 GE execute 时） |

**记忆法**：`PreAttributeChange` 是"看门人"（只夹取），`PostGameplayEffectExecute` 是"会计"（做结算）。

### 认知 2：Meta 属性（Damage / Healing）**绝对不要复制**

这是新手最常犯的带宽错误。`Damage` 不是角色的"状态"，它只是一个**从 GE 传进来的瞬时数值**：

```
服务器：GE_Damage 执行 → Damage 被写入 30 → PostGameplayEffectExecute 读取它 → 扣 Health → 立刻清零
客户端：Damage 永远是 0（因为它根本没复制，也不需要）
```

如果你给 `Damage` 加了复制：每帧复制一个恒为 0 的浮点数，纯浪费带宽。

**判断标准**：这个属性是否需要"客户端读到一个持续存在的状态值"？需要 → 复制（Health、MaxHealth）；只是一个瞬时输入 → 不复制（Damage、Healing）。

### 认知 3：`DOREPLIFETIME_CONDITION_NOTIFY` 里那个 `_NOTIFY` 是干什么的

对比一下：

```cpp
DOREPLIFETIME(UMyAttributeSet, Health);                    // 只复制值，不触发 OnRep
DOREPLIFETIME_CONDITION_NOTIFY(UMyAttributeSet, Health, COND_None, REPNOTIFY_Always);  // 复制并触发 OnRep
```

区别在最后那个 `REPNOTIFY_Always`：它让 `OnRep_Health` **即使值没有变化也调用**。为什么需要？

因为 GAS 的属性是"BaseValue + 一堆修正器（Modifier）"聚合出来的。**最终 CurrentValue 可能相同，但 BaseValue 变了**（例如"移除了 +10 的 buff，同时对面 -10 的 debuff 也到期了"）。如果只在值变化时通知，客户端就会漏掉这次 BaseValue 同步，导致**后续计算基准错误**——这类 bug 表现为"跑一会儿之后客户端血量算错"，极难排查。

所以 **AttributeSet 的属性复制一律用 `DOREPLIFETIME_CONDITION_NOTIFY` + `REPNOTIFY_Always`**，这是 GAS 的既定约定。

### 认知 4：UE5.8 有两处 API 与网上教程不一致（我逐条核实过）

| 网上的常见写法 | UE5.8 的实际情况 | 后果 |
|---|---|---|
| `GetAbilitySystemComponent()->GetSetByCallerMagnitude(Tag)` | ❌ **ASC 上没有这个方法了**。SetByCaller 只能从 `FGameplayEffectSpec` 上取，或直接用 `Data.EvaluatedData.Magnitude` | 编译失败 |
| `virtual void PostGameplayEffectExecute(FGameplayEffectModCallbackData& Data)` | 5.8 的签名是 **`const FGameplayEffectModCallbackData& Data`**（`AttributeSet.h:206`） | 编译失败（"does not override any base class method"） |
| `ATTRIBUTE_ACCESSORS(Class, Prop)` | 引擎里**只有注释里的示例**，实际提供的是 `ATTRIBUTE_ACCESSORS_BASIC`（`AttributeSet.h:465`） | 编译失败（宏未定义） |

正确写法都在下面。

---

## 1. 前置：两个 `Build.cs` 的依赖

### 1.1 `Plugins/GameFeatures/IDFCore/Source/IDFCore/IDFCore.Build.cs`

**先说明一个容易搞混的点**：这个 `Build.cs` 在**插件向导生成时就已经写好，而且已经包含了本节需要的 GAS 依赖**。你只需要核对它，不需要重写。

向导生成的现状（`IDFCore.Build.cs`）：

```csharp
public class IDFCore : ModuleRules
{
    public IDFCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(new string[] { /* 向导留空，保持空 */ });
        PrivateIncludePaths.AddRange(new string[] { /* 向导留空，保持空 */ });

        // ★ 你已经在向导的依赖列表里勾选了 GAS，所以这里是齐的
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "InputCore",
            "GameplayAbilities",
            "GameplayTags",
            "GameplayTasks",
        });

        PrivateDependencyModuleNames.AddRange(new string[] {
            "CoreUObject", "Engine", "Slate", "SlateCore",
        });

        DynamicallyLoadedModuleNames.AddRange(new string[] { /* 保持空 */ });
    }
}
```

**核对要点**（只需确认三件事）：

| 检查项 | 期望 | 为什么 |
|---|---|---|
| `GameplayAbilities` / `GameplayTags` / `GameplayTasks` 在 **`PublicDependencyModuleNames`** 里 | ✅ | 因为 `IDFCore/Public/` 下的头文件（如 `IDFHealthSet.h`）会 include GAS 头文件，必须作为 Public 依赖传递给"依赖 IDFCore 的模块"（比如主模块）。写成 Private 会让主模块编译 `IDFHealthSet.h` 时报"找不到 GAS 类型" |
| `GameplayTasks` 存在 | ✅ | `UAbilityTask` 继承 `UGameplayTask`；UBT 不会自动暴露传递依赖，漏了会报 `Cannot open include file: 'GameplayTask.h'` |
| `GameplayTags` 在这里（不是插件面板里开的） | ✅ | 它是**引擎内置模块**（`Engine/Source/Runtime/GameplayTags`），不是插件 |

> `PublicIncludePaths` / `PrivateIncludePaths` 可以留空——UBT 默认已把模块根、`Public/`、`Private/` 加入搜索路径，不需要手写。

### 1.2 `Source/Identity5/Identity5.Build.cs`（主模块）

**这一处你大概也已经改好了**（`Identity5.Build.cs` 里 `PrivateDependencyModuleNames` 已包含 `"IDFCore"`）。核对一下即可：

```csharp
PrivateDependencyModuleNames.AddRange(new string[]
{
    "IDFCore",   // ← 主模块要用 IDFCore 里的 GAS 骨架
});
```

> **依赖方向核对**（设计文档 2.1 节）：主模块 ──▶ 插件；插件之间 ──▶ `IDFCore`。**没有任何插件依赖主模块**，这就是这套划分的意义。
>
> **顺带说明**：主模块的 `PublicDependencyModuleNames` 里目前也有 `GameplayAbilities` / `GameplayTags` / `GameplayTasks`（向导阶段留下的）。保留它们**无害**（主模块将来自己写 GAS 相关代码时会用到），但严格来说，主模块只通过 `IDFCore` 间接使用 GAS 就够了。**建议保留**，避免以后想写一行 GAS 代码又要回来加依赖。

改完（或核对完）两个 `Build.cs` → **关闭编辑器** → 编译 → 打开编辑器（让 UBT 重新生成项目文件）。

---

## 2. 要建/要改的文件

**先看清现状**——插件向导已经生成了 4 个文件（**不要重复创建、也不要改错名**）：

```
Plugins/GameFeatures/IDFCore/
├── IDFCore.uplugin                     ✅ 已生成（改过 ExplicitlyLoaded，见 -1.3）
├── Content/
│   └── IDFCore.uasset                  ✅ 已生成（GameFeature 数据资产，引擎激活入口）
├── Resources/
│   └── Icon128.png                     ✅ 已生成
└── Source/IDFCore/
    ├── IDFCore.Build.cs                ✅ 已生成（含 GAS 依赖）
    ├── Public/
    │   └── IDFCoreModule.h             ✅ 已生成（模块入口，class FIDFCoreModule）
    └── Private/
        └── IDFCoreModule.cpp           ✅ 已生成（IMPLEMENT_MODULE(FIDFCoreModule, IDFCore)）
```

> ⚠️ **注意命名**：模块入口是 **`IDFCoreModule.h` / `IDFCoreModule.cpp`**，**不存在** `IDFCore.h` / `IDFCore.cpp` 这种文件（向导不为插件生成与插件同名的入口文件）。本节要新增的是**另外的文件**，放在子目录里。

**本节要新增的文件**（全部是新建，不覆盖向导产物）：

```
Plugins/GameFeatures/IDFCore/Source/IDFCore/
├── Public/
│   ├── IDFCoreModule.h                ← 已有，不动
│   ├── Logging/
│   │   └── IDFCoreLog.h               ← 新增：LogIDFNet / LogIDFAbility / LogIDFAttribute / LogIDFInteraction
│   ├── AbilitySystem/
│   │   ├── IDFAbilitySystemComponent.h ← 新增
│   │   └── IDFHealthSet.h              ← 新增
│   └── Debug/
│       └── IDFGasTestActor.h           ← 新增（临时验证载体，S07 后并入角色基类）
└── Private/
    ├── IDFCoreModule.cpp              ← 已有，不动
    ├── Logging/
    │   └── IDFCoreLog.cpp             ← 新增
    ├── AbilitySystem/
    │   ├── IDFAbilitySystemComponent.cpp ← 新增
    │   └── IDFHealthSet.cpp              ← 新增
    └── Debug/
        └── IDFGasTestActor.cpp         ← 新增
```

**配套要改的既有文件**（日志类别迁移，见 -1.6）：

```
Source/Identity5/Public/Logging/Identity5Log.h    ← 删掉 4 个已迁走的类别声明，只留 LogIdentity5
Source/Identity5/Private/Logging/Identity5Log.cpp ← 同步删掉 4 个 DEFINE_LOG_CATEGORY
```

> **include 路径说明**：UBT 会把模块根、`Public/`、`Private/` 都加入 include 搜索路径。所以 include 时写**从 include 根起的相对路径**，例如：
> ```cpp
> #include "AbilitySystem/IDFHealthSet.h"   // ✅
> #include "Logging/IDFCoreLog.h"          // ✅
> #include "IDFCoreModule.h"               // ✅（模块根下的文件）
> ```
> 不要写 `#include "../../Public/AbilitySystem/IDFHealthSet.h"` 这种相对路径。



---

## 3. `UIDFHealthSet`（本节重点）

### 3.1 头文件

```cpp
// IDFHealthSet.h
#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"              // UAttributeSet 与 ATTRIBUTE_ACCESSORS_BASIC
#include "AbilitySystemComponent.h"    // GAMEPLAYATTRIBUTE_REPNOTIFY 展开需要 ASC 的完整定义
#include "IDFHealthSet.generated.h"

/**
 * 角色生命属性集。
 *
 * 属性设计（对应设计文档第 2 轮 9.2 节）：
 *   Health     —— 当前生命值。需要复制：客户端要显示血条、判定受伤/倒地
 *   MaxHealth  —— 最大生命值。需要复制：客户端 UI 要用
 *   Damage     —— 【Meta 属性】伤害输入。不复制：它只是 GE 传进来的瞬时值，
 *                 在 PostGameplayEffectExecute 里被消费掉后立即清零
 *   Healing    —— 【Meta 属性】治疗输入。同上
 *
 * ⚠️ Meta 属性的关键约定：它们不参与复制，且每次结算后必须清零，
 *    否则下一次结算会读到上一次的残留值（经典 bug：第一次扣 30，第二次扣 60）。
 */
UCLASS()
class IDFCORE_API UIDFHealthSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    UIDFHealthSet();

    // ── 属性声明 ──
    // 所有属性都用 FGameplayAttributeData（它内部区分 BaseValue / CurrentValue，
    // 这是 GAS 能"叠加修正器"的基础 —— 不要用裸 float）。
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "IDF|Health")
    FGameplayAttributeData Health;

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "IDF|Health")
    FGameplayAttributeData MaxHealth;

    // Meta 属性：无 ReplicatedUsing —— 它们不复制
    UPROPERTY(BlueprintReadOnly, Category = "IDF|Health")
    FGameplayAttributeData Damage;

    UPROPERTY(BlueprintReadOnly, Category = "IDF|Health")
    FGameplayAttributeData Healing;

    // ── 访问器宏 ──
    // ATTRIBUTE_ACCESSORS_BASIC 会生成 4 个函数（已核实 AttributeSet.h:465）：
    //   static FGameplayAttribute GetHealthAttribute();   ← 给 GE 与代码定位属性用
    //   float GetHealth() const;                          ← 读当前值
    //   void  SetHealth(float);                           ← 写 BaseValue（会走 SetNumericAttributeBase）
    //   void  InitHealth(float);                          ← 初始化 BaseValue + CurrentValue（只该在初始化时用）
    ATTRIBUTE_ACCESSORS_BASIC(UIDFHealthSet, Health);
    ATTRIBUTE_ACCESSORS_BASIC(UIDFHealthSet, MaxHealth);
    ATTRIBUTE_ACCESSORS_BASIC(UIDFHealthSet, Damage);
    ATTRIBUTE_ACCESSORS_BASIC(UIDFHealthSet, Healing);

protected:
    // ── 网络复制 ──
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ── 看门人：只做夹取 ──
    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
    virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;

    // ── 会计：做结算 ──
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

    // ── RepNotify ──
    UFUNCTION()
    void OnRep_Health(const FGameplayAttributeData& OldHealth);

    UFUNCTION()
    void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);
};
```

⚠️ **三个必须点**：

1. **`#include "IDFHealthSet.generated.h"` 必须是最后一个 include**（顺序错了 UHT 会报奇怪的错）；
2. **`GAMEPLAYATTRIBUTE_REPNOTIFY` 需要 `AbilitySystemComponent.h` 的完整定义**——因为宏展开里会调用 `GetOwningAbilitySystemComponentChecked()->SetBaseAttributeValueFromReplication(...)`（我核实过宏体，`AttributeSet.h:402-406`）。只前向声明 `UAbilitySystemComponent` 会编译失败。
3. **Meta 属性不要加 `ReplicatedUsing`**，也不要放进 `GetLifetimeReplicatedProps`。

### 3.2 `.cpp` 实现

```cpp
// IDFHealthSet.cpp
#include "AbilitySystem/IDFHealthSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffectExtension.h"          // FGameplayEffectModCallbackData（5.8 里定义在这个文件）
#include "Net/UnrealNetwork.h"                // DOREPLIFETIME_CONDITION_NOTIFY
#include "Logging/IDFCoreLog.h"               // LogIDFAttribute（日志类别已迁到 IDFCore，见 -1.6 节）

UIDFHealthSet::UIDFHealthSet()
{
    // 用 Init 系列初始化"基础值"。
    // 为什么不直接在头文件里写 = 100.f？
    //   因为 FGameplayAttributeData 不是普通 float，它有独立的 BaseValue/CurrentValue。
    //   用 InitXxx() 才能同时设置两者（ATTRIBUTE_ACCESSORS_BASIC 生成的 Initter）。
    InitMaxHealth(100.f);
    InitHealth(100.f);

    InitDamage(0.f);
    InitHealing(0.f);
}

void UIDFHealthSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // ⚠️ AttributeSet 的属性一律用 DOREPLIFETIME_CONDITION_NOTIFY + REPNOTIFY_Always
    //    原因：最终 CurrentValue 可能没变但 BaseValue 变了（buff/debuff 抵消），
    //          漏掉这次同步会导致客户端后续计算基准错误。
    DOREPLIFETIME_CONDITION_NOTIFY(UIDFHealthSet, Health,    COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UIDFHealthSet, MaxHealth, COND_None, REPNOTIFY_Always);

    // ❌ 不要写 Damage / Healing —— Meta 属性不复制
}

void UIDFHealthSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
    // GAMEPLAYATTRIBUTE_REPNOTIFY 做两件事：
    //   1) 找到 Health 对应的 FProperty
    //   2) 调用 ASC->SetBaseAttributeValueFromReplication(...)
    //      让 GAS 的聚合器知道"这个值是被网络同步过来的"，从而正确更新 CurrentValue
    // 参数顺序：(本类, 属性名, 旧值)
    GAMEPLAYATTRIBUTE_REPNOTIFY(UIDFHealthSet, Health, OldHealth);
}

void UIDFHealthSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UIDFHealthSet, MaxHealth, OldMaxHealth);
}

void UIDFHealthSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);

    // 只做夹取。注意这里读的是 GetMaxHealth()（当前值），不是 GetMaxHealthAttribute()。
    if (Attribute == GetHealthAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
    }
}

void UIDFHealthSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
    Super::PreAttributeBaseChange(Attribute, NewValue);

    // 规则必须与 PreAttributeChange 保持一致，否则会出现
    // "BaseValue 超出范围但 CurrentValue 被夹住"的不一致状态。
    if (Attribute == GetHealthAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
    }
}

void UIDFHealthSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    // ── 第一步：取出"谁打了谁"的上下文 ──
    // Data 里三个成员（已核实 GameplayEffectExtension.h:26-29）：
    //   const FGameplayEffectSpec& EffectSpec  —— 这次效果的数据（含 SetByCaller 表）
    //   FGameplayModifierEvaluatedData& EvaluatedData —— 本次被计算的修正（含 Magnitude）
    //   UAbilitySystemComponent& Target        —— 被施加者
    FGameplayEffectContextHandle Context = Data.EffectSpec.GetContext();
    UAbilitySystemComponent* SourceASC = Context.GetOriginalInstigatorAbilitySystemComponent();
    const AActor* SourceActor = SourceASC ? SourceASC->GetAvatarActor() : nullptr;

    // 被施加者的 Controller / Character（可能为空：例如伤害来自环境陷阱）
    AController* TargetController = nullptr;
    ACharacter* TargetCharacter = nullptr;
    if (Data.Target.AbilityActorInfo.IsValid() && Data.Target.AbilityActorInfo->AvatarActor.IsValid())
    {
        AActor* TargetActor = Data.Target.AbilityActorInfo->AvatarActor.Get();
        TargetController = Data.Target.AbilityActorInfo->PlayerController.Get();
        TargetCharacter = Cast<ACharacter>(TargetActor);
    }

    // ── 第二步：结算 Damage ──
    if (Data.EvaluatedData.Attribute == GetDamageAttribute())
    {
        // 伤害值来源：优先取 SetByCaller（GE 里配置的 "Data.Damage"），
        // 否则退化为本修正的 Magnitude。
        //
        // ⚠️ 已核实：UE5.8 的 UAbilitySystemComponent 上【没有】GetSetByCallerMagnitude()。
        //    它现在只在 FGameplayEffectSpec 上（GameplayEffect.h:1120/1123）：
        //      float GetSetByCallerMagnitude(FGameplayTag DataTag, bool WarnIfNotFound = true,
        //                                    float DefaultIfNotFound = 0.f) const;
        //    请先把 "Data.Damage" 标签加进 IDFGameplayTags（S06 会做），
        //    本节先用 Magnitude 跑通链路。
        const float LocalDamage = Data.EvaluatedData.Magnitude;

        // 应用伤害：注意这里是"减"，且 SetHealth 会走 PreAttributeChange 夹取
        SetHealth(FMath::Clamp(GetHealth() - LocalDamage, 0.f, GetMaxHealth()));

        UE_LOG(LogIDFAttribute, Verbose, TEXT("[生命] %s 受到 %.1f 伤害，剩余 %.1f / %.1f（来源：%s）"),
            TargetCharacter ? *TargetCharacter->GetName() : TEXT("<未知>"),
            LocalDamage,
            GetHealth(),
            GetMaxHealth(),
            SourceActor ? *SourceActor->GetName() : TEXT("<环境>"));

        // 触发生命变化事件（让 UI / 表现层订阅，而不是让 UI 直接轮询属性）
        // 这里先只打日志，S07 接入 UI 时再换成广播 GameplayMessage
        if (GetHealth() <= 0.f)
        {
            UE_LOG(LogIDFAttribute, Log, TEXT("[生命] %s 生命归零"),
                TargetCharacter ? *TargetCharacter->GetName() : TEXT("<未知>"));
        }
    }

    // ── 第三步：结算 Healing ──
    if (Data.EvaluatedData.Attribute == GetHealingAttribute())
    {
        const float LocalHealing = Data.EvaluatedData.Magnitude;
        SetHealth(FMath::Clamp(GetHealth() + LocalHealing, 0.f, GetMaxHealth()));

        UE_LOG(LogIDFAttribute, Verbose, TEXT("[生命] 治疗 %.1f，当前 %.1f / %.1f"),
            LocalHealing, GetHealth(), GetMaxHealth());
    }

    // ── 第四步（关键）：清空 Meta 属性 ──
    // 不清零的后果：下一次 GE 执行时，如果它没有写入 Damage，
    // PostGameplayEffectExecute 会读到上次的残留值 → 重复扣血。
    // 这是 GAS 新手最经典的一个 bug。
    if (Data.EvaluatedData.Attribute == GetDamageAttribute())
    {
        SetDamage(0.f);
    }
    if (Data.EvaluatedData.Attribute == GetHealingAttribute())
    {
        SetHealing(0.f);
    }
}
```

### 3.3 三个"为什么"（面试会问）

| 写法 | 为什么 |
|---|---|
| 属性用 `FGameplayAttributeData` 而不是 `float` | GAS 需要区分 BaseValue 与 CurrentValue 才能叠加修正器（`+10 攻击`、`50% 减速`）。裸 float 无法参与聚合计算 |
| `SetHealth()` 而不是 `Health = xxx` | `SetXxx()` 会走 `ASC->SetNumericAttributeBase()`（宏展开里能看到，`AttributeSet.h:441-449`），从而触发聚合器重算与 `PreAttributeChange`。直接赋值会绕过整套机制 |
| `PostGameplayEffectExecute` 里用 `SetHealth(...)` 而不是 `Data.EvaluatedData.Magnitude = ...` | 后者是"修改这一次修正的值"，只在该次 GE 生效；前者是"我要改当前血量"，语义清晰。两者都能用，但**改 Meta 属性用后者、改真实属性用前者**更不容易混 |

---

## 4. `UIDFAbilitySystemComponent`（本节只做薄壳）

S05 阶段不要给它堆功能。先建一个**空壳**，目的是：
1. 把"项目自己的 ASC 类"这个位置占住（S06 的 `AbilityInputTagPressed` 等会加进来）；
2. 在构造函数里设置一些全局默认（例如 `SetIsReplicated(true)`、`ReplicationMode`）。

```cpp
// IDFAbilitySystemComponent.h
#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "IDFAbilitySystemComponent.generated.h"

/**
 * 项目自定义 ASC。
 *
 * S05 阶段它是一个薄壳，只负责：
 *   1) 显式打开复制
 *   2) 提供统一的调试打印入口
 * 后续会在它上面实现：
 *   S06 —— 能力输入转发（AbilityInputTagPressed/Released）、UIDFAbilitySet 装配
 *   S07 —— NotifyAbilityFailed 统一失败反馈
 *   S11 —— 属性变化订阅的统一出口
 */
UCLASS()
class IDFCORE_API UIDFAbilitySystemComponent : public UAbilitySystemComponent
{
    GENERATED_BODY()

public:
    UIDFAbilitySystemComponent();

    /** 打印当前 ASC 的能力、标签、关键属性（由 IDF.Debug.GAS 1 触发）
     *  刻意【不加 const】：调试函数没必要 const，且能避免碰到 GAS 内部
     *  某些 const 重载不完整的坑（宁可少一个 const 也不要卡在编译错误上）。 */
    void PrintDebugInfo();
};
```

```cpp
// IDFAbilitySystemComponent.cpp
#include "AbilitySystem/IDFAbilitySystemComponent.h"
#include "AbilitySystem/IDFHealthSet.h"
#include "Logging/IDFCoreLog.h"

UIDFAbilitySystemComponent::UIDFAbilitySystemComponent()
{
    // ReplicationMode 的三种取值（GAS 官方定义）：
    //   Full    —— 所有 GE 都复制到所有客户端（开销最大，适合单人/少量单位）
    //   Mixed   —— 只有自己拥有的角色收到完整 GE 信息，其他人只收 GE 结果（玩家角色推荐值）
    //   Minimal —— 只复制 GameplayTag 与 GameplayCue（AI 单位推荐值）
    //
    // 本项目：求生者/监管者都用 Mixed —— 自己受控角色全量预测，其他人只看到结果。
    SetIsReplicated(true);
    ReplicationMode = EGameplayEffectReplicationMode::Mixed;
}

void UIDFAbilitySystemComponent::PrintDebugInfo()
{
    UE_LOG(LogIDFAbility, Log, TEXT("========== IDF ASC 调试信息 =========="));

    // 1) 属性集
    int32 SetIndex = 0;
    for (const UAttributeSet* Set : GetSpawnedAttributes())
    {
        UE_LOG(LogIDFAbility, Log, TEXT("  属性集[%d] = %s"), SetIndex++,
            Set ? *GetNameSafe(Set->GetClass()) : TEXT("<空>"));
    }

    // 2) 已授予的能力
    //    注意：FGameplayAbilitySpec 的遍历要用 GetActivatableAbilities()，
    //    它返回 const TArray<FGameplayAbilitySpec>&
    int32 AbilityIndex = 0;
    for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
    {
        UE_LOG(LogIDFAbility, Log, TEXT("  能力[%d] = %s（等级 %d，是否激活 %s）"),
            AbilityIndex++,
            Spec.Ability ? *GetNameSafe(Spec.Ability->GetClass()) : TEXT("<空>"),
            Spec.Level,
            Spec.IsActive() ? TEXT("是") : TEXT("否"));
    }

    // 3) 当前持有的标签（Loose + 能力/GE 授予的）
    UE_LOG(LogIDFAbility, Log, TEXT("  当前标签   : %s"), *GetOwnedGameplayTags().ToStringSimple());

    // 4) 生命属性（若该角色挂了 HealthSet）
    if (const UIDFHealthSet* HealthSet = GetSet<UIDFHealthSet>())
    {
        UE_LOG(LogIDFAbility, Log, TEXT("  生命       : %.1f / %.1f"),
            HealthSet->GetHealth(), HealthSet->GetMaxHealth());
    }
    else
    {
        UE_LOG(LogIDFAbility, Log, TEXT("  生命       : 未挂载 UIDFHealthSet"));
    }

    UE_LOG(LogIDFAbility, Log, TEXT("======================================"));
}
```

⚠️ **三个 API 细节**：

| 点 | 说明 |
|---|---|
| `GetSpawnedAttributes()` | 返回 `const TArray<UAttributeSet*>&`。注意属性集是通过 `AddAttributeSetSubobject` / `AddSet<T>()` 注册进来的 |
| `GetActivatableAbilities()` | 返回 `const TArray<FGameplayAbilitySpec>&`。**不要**用 `ActivatableAbilities.Items`（那是 UE4 的写法） |
| `GetSet<UIDFHealthSet>()` | ASC 的模板便捷函数，等价于"找到该类型的属性集"。比 `FindAttributeSet` 好用 |

---

## 5. 验证手段（S05 特有的难点）

**问题**：S05 阶段我们还没有 `AIDFCharacterBase`（S07 才建），所以**没有实体挂载这个 ASC 和 HealthSet**。那怎么验证？

**方案**：临时用一个最小 Actor 来挂载。**它不是垃圾代码**——S07 的 `AIDFCharacterBase` 会直接继承这套挂载逻辑。

📁 **临时新建** `Plugins/GameFeatures/IDFCore/Source/IDFCore/Public/Debug/IDFGasTestActor.h` 与 `Private/Debug/IDFGasTestActor.cpp`

> 为什么测试 Actor 也放 `IDFCore`：它要挂载 `UIDFAbilitySystemComponent` 与 `UIDFHealthSet`，放同一个模块最省事（不需要主模块依赖、也不需要额外的公共头暴露）。S07 建立 `AIDFCharacterBase` 后，本类可删除或保留为测试夹具。

```cpp
// IDFGasTestActor.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"       // 让外部能通过接口拿到 ASC
#include "IDFGasTestActor.generated.h"

class UIDFAbilitySystemComponent;
class UIDFHealthSet;

/**
 * 【临时验证用】挂载 ASC + HealthSet 的最小 Actor。
 *
 * 用途：S05 阶段验证属性集注册与网络复制是否正常。
 * 生命周期：S07 建立 AIDFCharacterBase 后，本类可删除或保留为测试夹具。
 */
UCLASS()
class IDFCORE_API AIDFGasTestActor : public AActor, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    AIDFGasTestActor();

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "IDF|GAS")
    TObjectPtr<UIDFAbilitySystemComponent> AbilitySystemComponent;

    // ★ 属性集成员建议用【裸指针 UPROPERTY()】，不要用 TObjectPtr。
    //   原因：GAS 的接口大量使用模板 T*（AddAttributeSetSubobject / AddSet / GetSet），
    //   而 TObjectPtr -> T* 的隐式转换【不参与模板实参推导】，每处调用都得写 .Get()。
    //   裸指针同样是 GC 安全的（被 UPROPERTY 标记就会被引用追踪），且可读性更好。
    //   注意：裸指针声明【不要】写 = nullptr 初始化（UHT 会报错），保持不初始化即可。
    UPROPERTY()
    UIDFHealthSet* HealthSet;
};
```

> **注意点**：裸指针的 `UPROPERTY()` **不要写 `= nullptr` 初始化**——UHT 对裸指针成员的内联初始化会报错，保持不初始化即可。
>
> 如果你更愿意保留 `TObjectPtr<UIDFHealthSet>`，**也可以**，只需把调用处改成 `AddAttributeSetSubobject(HealthSet.Get())`（`.Get()` 把 `TObjectPtr` 转成裸指针）。两种写法都对，裸指针的好处是后面所有 GAS 接口调用都不用再写 `.Get()`。

> **为什么 `TObjectPtr` 在这里会添麻烦**：`TObjectPtr<T>` 定义了 `operator T*()`（`ObjectPtr.h:722`，我核实过），所以**普通函数传参**没问题；但 `AddAttributeSetSubobject` 是 `template<class T> const T* AddAttributeSetSubobject(T* Subobject)`，**模板实参推导不会应用用户自定义类型转换**，于是推导失败。
>
> 你遇到的报错就是这个。两种解法都行：
> - **推荐**：成员改成裸指针 `UIDFHealthSet*`（本文档已改）；
> - 或者保留 `TObjectPtr`，调用处写 `AddAttributeSetSubobject(HealthSet.Get())`。

```cpp
// IDFGasTestActor.cpp
#include "Debug/IDFGasTestActor.h"
#include "AbilitySystem/IDFAbilitySystemComponent.h"
#include "AbilitySystem/IDFHealthSet.h"
#include "Net/UnrealNetwork.h"
#include "Logging/IDFCoreLog.h"

AIDFGasTestActor::AIDFGasTestActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;                                   // ★ 必须：否则属性不会复制

    AbilitySystemComponent = CreateDefaultSubobject<UIDFAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    HealthSet = CreateDefaultSubobject<UIDFHealthSet>(TEXT("HealthSet"));

    // 复制模式在 ASC 的构造函数里已设为 Mixed
}

UAbilitySystemComponent* AIDFGasTestActor::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

void AIDFGasTestActor::BeginPlay()
{
    Super::BeginPlay();

    // ── 初始化顺序（S07 会把这套逻辑搬进 AIDFCharacterBase）──
    // 1) 先设 Owner/Avatar：ASC 靠它找到"我是谁的技能系统"
    // 2) 再把属性集注册进 ASC 的 SpawnedAttributes 列表
    // 顺序反了会导致属性集找不到 Owner，PostGameplayEffectExecute 里的 AbilityActorInfo 为无效
    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->InitAbilityActorInfo(this, this);

        // ★ 关键：必须传【裸指针】。
        //   HealthSet 成员的类型是 TObjectPtr<UIDFHealthSet>，而 AddAttributeSetSubobject
        //   的形参是模板的 T*，模板实参推导不会走 TObjectPtr -> T* 的转换，所以必须显式 .Get()。
        //   报错形如：
        //     error C2784/C2664: 无法从 "TObjectPtr<UIDFHealthSet>" 推导出 "T *" 的模板参数
        AbilitySystemComponent->AddAttributeSetSubobject(HealthSet.Get());

        UE_LOG(LogIDFAbility, Log, TEXT("[GAS 测试] %s 初始化完成（服务器=%s），生命 %.1f/%.1f"),
            *GetName(),
            HasAuthority() ? TEXT("是") : TEXT("否"),
            HealthSet->GetHealth(), HealthSet->GetMaxHealth());
    }
}
```

**三种等价写法，任选一种（都能正确注册，`AddSpawnedAttribute` 内部有去重）**：

| 写法 | 代码 | 说明 |
|---|---|---|
| **A（本文档采用）** | 构造函数 `CreateDefaultSubobject` + `BeginPlay` 里 `AddAttributeSetSubobject(HealthSet.Get())` | 属性集是 ASC 的**默认子对象**，随 Actor 一起序列化；显式注册进 `SpawnedAttributes`。语义最清楚 |
| B | 构造函数**不建**属性集，`BeginPlay` 里 `AbilitySystemComponent->AddSet<UIDFHealthSet>()` | 由 ASC 创建实例并返回 `const T*`（`AbilitySystemComponent.h:153-157`）。代码最短，但属性集不是"默认子对象"，蓝眼里不容易看到 |
| C | 构造函数只 `CreateDefaultSubobject`，**完全不调用**注册 | ASC 在 `OnRegister()` 里会遍历并注册自己的子对象。**能用但不推荐**——依赖隐式行为，出问题时很难定位 |

> ⚠️ **为什么会报模板推导错误**（这是 UE5 里非常高频的一类错误，值得记住）：C++ 模板形参 `T*` 在推导实参时**不会应用用户定义的类型转换**。`TObjectPtr<T>` 虽然定义了 `operator T*()`（我核实过，`ObjectPtr.h:722`），但模板推导只看类型本身能否匹配 `T*`——不能，于是推导失败。**遇到"无法从 `TObjectPtr<X>` 推导/转换为 `X*`"就加 `.Get()`**，这是通用解法。

---

## 6. 自测清单（本节 DoD）

> 📄 **前置动作见 [S05-verify-level.md](S05-verify-level.md)**：PIE 需要一张地图，所以先建 `Content/IDF/Maps/L_MilitaryFactory`（Basic 模板 + 地面 + PlayerStart），再把 `AIDFGasTestActor` 拖进去。那张图**不是一次性的**——S15 会在同一张图上做军工厂灰盒。

| # | 验证项 | 期望结果 |
|---|---|---|
| 0 | **插件已加载** | 编辑器 → 编辑 → 插件，搜 `IDF Core` 显示"已启用"；`Saved/Logs/Identity5.log` 里搜 `IDFCore` 能看到模块加载记录 |
| 0b | **关卡可用** | `Content/IDF/Maps/L_MilitaryFactory` 能打开、有地面与光照；`Project Settings → Maps & Modes` 三个字段指向它 |
| 1 | 编译通过 | 无 Error。**特别确认没有报 `PostGameplayEffectExecute` 未覆盖基类**（那就是签名写错了，漏了 `const`）；也没有 `unresolved external symbol`（那就是 `IDFCORE_API` 没改对） |
| 2 | 类被反射注册 | Content Browser 切到 **C++ Classes** 能看到 `IDFGasTestActor`，能拖进关卡并生成。**注意**：这些类来自 `IDFCore` 模块，若搜不到说明插件没加载、没编译成功或 API 宏写错 |
| 3 | 属性集注册成功 | PIE 后 Output Log 里看到 `[GAS 测试] XXX 初始化完成（服务器=是），生命 100.0/100.0` |
| 4 | 属性集列表非空 | 在测试 Actor 的 `BeginPlay` 末尾临时调一次 `PrintDebugInfo()`，日志里 `属性集[0] = IDFHealthSet` |
| 5 | 标签查询不崩 | 同上，`当前标签` 一行有输出（可能是空字符串，但不应崩溃） |
| 6 | **客户端也能读到属性**（核心） | 用 **Play As Listen Server + 4 个客户端** 跑：在**客户端窗口**的日志里，`生命 100.0/100.0` 也能打印出来，且 `服务器=否`。如果客户端是 `0.0/0.0` → 见 [S05-verify-level.md](S05-verify-level.md) 第 6.3 节的排查表 |
| 7 | 修改属性会触发夹取 | 临时在服务器 `BeginPlay` 里调 `HealthSet->SetHealth(150.f)` → 日志里的生命应该是 **100.0**（被 `PreAttributeChange` 夹回 MaxHealth），不是 150 |
| 8 | Meta 属性确实不复制 | 代码审查：`GetLifetimeReplicatedProps` 里**没有** `Damage` / `Healing` |

### 验证项 6 失败时的排查顺序（这是本节最可能卡住的地方）

| 症状 | 原因 | 修复 |
|---|---|---|
| 客户端 `0.0/0.0`，服务器 `100.0/100.0` | `bReplicates` 为 false | 构造函数里 `bReplicates = true;`（`AActor` 默认是 false） |
| 客户端 0，但 `bReplicates` 已开 | 忘了在 `GetLifetimeReplicatedProps` 里写 `DOREPLIFETIME_CONDITION_NOTIFY` | 检查 3.2 节的实现 |
| 客户端 0，复制宏也写了 | `OnRep` 函数没加 `UFUNCTION()` | RepNotify 函数**必须是 UFUNCTION**，否则不会被调用（且不报错，非常隐蔽） |
| 客户端 0，`UFUNCTION` 也有 | `AddAttributeSetSubobject` 没调用，或调用在 `InitAbilityActorInfo` **之前** | 调整 `BeginPlay` 里的顺序 |
| 客户端数值对但 UI 不更新 | `OnRep` 里忘了调 `GAMEPLAYATTRIBUTE_REPNOTIFY` | 补上（它负责把同步值告知聚合器） |
| **只在 PIE 单窗口正常** | 你其实没在客户端窗口看日志，看的是服务器窗口 | 确认日志来源；用 `IDF.Debug.DumpWorld` 确认当前是 `Client` 还是 `ListenServer` |

---

## 7. 常见坑汇总

| 现象 | 原因 | 处理 |
|---|---|---|
| `PostGameplayEffectExecute` 报"没有覆盖任何基类虚函数" | 5.8 签名是 `const FGameplayEffectModCallbackData&` | 加 `const` |
| `FGameplayEffectModCallbackData` 未定义 | 头文件位置变了（5.8 里 `GameplayEffectExtension.h` 只剩这个结构体定义，游戏扩展类已移除） | `#include "GameplayEffectExtension.h"` |
| `ATTRIBUTE_ACCESSORS` 未定义 | 引擎里只有注释示例，实际宏名是 `ATTRIBUTE_ACCESSORS_BASIC` | 用 `ATTRIBUTE_ACCESSORS_BASIC`，或按注释自己定义一份 |
| `GetSetByCallerMagnitude` 找不到 | **5.8 的 ASC 上没有这个方法** | 用 `Data.EvaluatedData.Magnitude`，或 `Data.EffectSpec.GetSetByCallerMagnitude(Tag)` |
| 链接错误 `unresolved external symbol UAbilitySystemComponent` | `Build.cs` 缺 `GameplayAbilities` / `GameplayTasks` | 补依赖并重新生成项目文件 |
| 编译报找不到 `GameplayTagContainer` | 缺 `GameplayTags` 依赖（它是引擎模块不是插件） | 加 `"GameplayTags"` |
| `SetHealth` 编译不过 | `ATTRIBUTE_ACCESSORS_BASIC` 里有 `GAMEPLAYATTRIBUTE_VALUE_SETTER`，它 `ensure(AbilityComp)` | 确认 ASC 已初始化（`AddAttributeSetSubobject` 已调用）；否则 `ensure` 会触发 |
| 编译错：`无法将类型 T* 与 TObjectPtr<UIDFHealthSet> 匹配` / `无法推导出 "T *" 的模板参数` | `AddAttributeSetSubobject` 的形参是模板 `T*`，而成员是 `TObjectPtr<UIDFHealthSet>`。**模板实参推导不应用用户自定义类型转换** | 传裸指针：`AddAttributeSetSubobject(HealthSet.Get())`。这是 UE5 通病——凡是"`TObjectPtr<X>` 传给要 `X*` 的（模板）参数"都加 `.Get()` |
| 编译错：`TObjectPtr<X>` 传给 `const X*` 形参 | 隐式转换只能到**非 const** `X*` | 用 `ToRawPtr()` 或先取 `X*` 局部变量（`.Get()` 得到的是 `X*`，赋给 `const X*` 是允许的） |
| 属性值"改了又变回去" | 你在客户端调用了 `SetHealth()` | 属性写入必须**只在服务器**；客户端写会被下一次复制覆盖 |
| Meta 属性越扣越多 | 结算后没清零 | `PostGameplayEffectExecute` 末尾 `SetDamage(0.f)` |
| 每次进 PIE 属性都是 100，改了不保留 | 正常：属性是运行时状态，不持久化 | 需要持久化时用 SaveGame，不在本轮范围 |
| `FindFieldChecked` 崩溃 | 属性名拼写与 `UPROPERTY` 不一致，或 `GENERATED_BODY()` 位置错 | 检查属性名在头文件与宏中完全一致 |
| `unresolved external symbol ...UIDFHealthSet::StaticClass()` | 类在 `IDFCore` 模块里，却写成了 `IDENTITY5_API` | 全部改成 `IDFCORE_API` |
| 插件代码"像是没编译"（改了不生效） | 插件没在 `.uproject` 注册 → UBT 不编译它 | 检查 `Identity5.uproject` 的 `Plugins` 数组里有 `IDFCore`；或插件面板里没启用 |
| 打包/运行时报"找不到 IDFCore 模块" | `.uplugin` 里 `LoadingPhase` 写成 `PreDefault`，早于 Engine 反射注册 | 改成 `Default` |
| `#include "AbilitySystem/IDFHealthSet.h"` 找不到 | 头文件没放在插件的 `Public/` 下（UBT 只把 `Public/`、`Private/` 及模块根加入 include 路径） | 检查目录层级；插件的公共头必须在 `Public/` |
| 主模块编译报找不到 `IDFHealthSet` | 主模块 `Build.cs` 没加 `"IDFCore"` | 在 `PrivateDependencyModuleNames` 里加 |
| 插件的 `Binaries/` 被提交进仓库 | 工程根 `/Binaries/` 是锚定规则，不匹配子目录 | 在 `.gitignore` 补 `/Plugins/GameFeatures/*/Binaries/` 与 `.../Intermediate/` |
| 构建失败：`Unable to find plugin 'GameplayTags' (referenced via ... IDFCore.uplugin)` | 把**引擎模块**写进了 `.uplugin` 的 `"Plugins"` 数组（那是**插件**依赖，必须是真实存在的插件） | 从 `.uplugin` 删掉该行；模块依赖写在 `Build.cs` 的 `DependencyModuleNames` |
| 构建失败：`Unable to find plugin 'XXX'` 且 XXX 是你的另一个插件 | 依赖的插件还没建 / 名字拼错 / 没在 `.uproject` 注册 | 先建那个插件，或从 `"Plugins"` 里移除；确认 `.uproject` 已注册 |
| Rider/VS 不索引插件代码（无补全、不能跳转） | `Intermediate/ProjectFiles/` 里没有该插件的 `.vcxproj`（工程文件过期） | 关闭编辑器 → 删 `Plugins/GameFeatures/<插件>/Binaries` 与 `Intermediate` → 右键 `.uproject` 重新生成工程文件 → Rider 重新加载 |
| 改了模块名后报奇怪的链接/加载错误 | `Binaries/` 里还留着**旧模块名**的 dll | 删掉该插件的 `Binaries` 与 `Intermediate` 后重新编译 |

---

## 8. 提交

```powershell
cd 'G:\Project\Unreal Projects\Identity5'

# 插件本体（.uplugin 由编辑器创建，通常已自动写入 .uproject）
git add Identity5.uproject `
        Plugins/GameFeatures/IDFCore/IDFCore.uplugin `
        Plugins/GameFeatures/IDFCore/Source/ `
        Config/DefaultEngine.ini

# 主模块的依赖声明
git add Source/Identity5/Identity5.Build.cs

git commit -m "feat(gas): 新建 IDFCore GameFeature 插件并建立最小 GAS 骨架（自定义 ASC + IDFHealthSet，含 Meta 属性结算与复制约定）"
```

> ⚠️ `Plugins/GameFeatures/IDFCore/Binaries/` 与 `Intermediate/` 会被 `.gitignore` 挡住（工程根的 `/Binaries/` 是锚定根目录的规则，所以插件的构建产物需要**单独忽略**）。检查一下：
> ```powershell
> git check-ignore -v Plugins/GameFeatures/IDFCore/Binaries/Win64/UnrealEditor-IDFCore.dll
> ```
> 若没有输出（说明没被忽略），在 `.gitignore` 的第 3 组里补一行：
> ```
> /Plugins/GameFeatures/*/Binaries/
> /Plugins/GameFeatures/*/Intermediate/
> ```
> 这一条很关键——否则每加一个角色插件就会往仓库里塞几百 MB 的 dll。

---

## 9. 简历点与面试问答

💼 **可以这样写**：

> **GAS 基础层**：实现自定义 `IDFAbilitySystemComponent`（ReplicationMode = Mixed）与 `IDFHealthSet`；区分持久属性（Health/MaxHealth，走 `DOREPLIFETIME_CONDITION_NOTIFY` + `REPNOTIFY_Always`）与 Meta 属性（Damage/Healing，不复制、结算后清零）；在 `PreAttributeChange`/`PreAttributeBaseChange` 统一夹取、在 `PostGameplayEffectExecute` 集中结算，使伤害/治疗/无敌/翻越免疫等规则只有一个落点。

**可能被追问的问题，先想清楚答案**：

1. **"为什么 Meta 属性不复制？"**
   → 它是 GE 的瞬时输入而非角色状态。复制它等于每帧同步一个恒为 0 的值，纯浪费带宽；而且客户端若读到非零值会误判自己受伤。

2. **"`PreAttributeChange` 和 `PostGameplayEffectExecute` 分别什么时候调用？"**
   → 前者在任何修改（含持续 GE）时调用，只做夹取，调用极频繁；后者只在 GE 执行时调用，做结算。混用会导致"结算逻辑被高频执行"或"夹取漏掉 BaseValue"。

3. **"为什么用 `DOREPLIFETIME_CONDITION_NOTIFY` 而不是 `DOREPLIFETIME`？"**
   → 因为 GAS 属性是 BaseValue + 修正器聚合的结果，CurrentValue 可能不变而 BaseValue 变了（buff/debuff 抵消）。`REPNOTIFY_Always` 保证即使值相同也通知客户端更新 BaseValue，否则客户端后续计算基准错误。

4. **"属性写入为什么必须只在服务器？"**
   → 属性是复制状态，客户端写入会被下一次复制覆盖，表现为"改了又变回去"。客户端只做预测显示与本地表现。

5. **"你怎么验证属性真的复制到了客户端？"**
   → Listen Server + 2 客户端，在**客户端窗口**调用 `PrintDebugInfo()` 比对生命值；并在客户端日志里确认 `HasAuthority()` 为 false。这也是为什么我在 S05 就要你搭好 5 窗口环境——**GAS 的绝大多数问题只在客户端暴露**。

---

## 10. 下一步

S05 完成后进 **S06：AbilitySet + InputConfig + 原生 GameplayTag**。到那一步你会第一次把"数据资产"接进 GAS，并且本节留下的两个 `TODO` 会被填上：

- `PostGameplayEffectExecute` 里的 SetByCaller 取值（需要 `Data.Damage` 标签）；
- `AIDFGasTestActor` 的挂载逻辑迁移到 `AIDFCharacterBase`（S07）。

> **提醒**：本节完成后请务必做**验证项 6**（客户端能读到属性）。如果跳过它直接往下走，你会在 S07 花几小时排查一个本来 5 分钟就能定位的问题——"属性在客户端是 0"是 GAS 里最高频的症状，而它的根因就在本节这几行配置里。
