# Identity5 实施路线图（自学驱动 / 我辅助，代码由你写）

> **这份文档是什么**：把 `design_analysis.md` 里那套架构，拆成**你可以一步一步自己动手实现**的施工顺序。
> **这份文档不是什么**：不是代码清单。每一步我**不给你可粘贴的成品代码**，只给你：要建什么文件、要写什么类、关键 API 叫什么、数据怎么流、怎么自测、以及最容易踩的坑。
> **你的角色**：亲手写每一行代码，遇到不懂的模块随时回来问我（问的时候把"我写到哪了 + 报错信息 + 你想不通的点"一起给我）。
> **我的角色**：讲清概念、指出 API、审你的代码、帮你定位报错、在被卡住时给最小可运行示例（只给卡住的那一小段）。

---

## 0. 怎么用这份路线图

### 0.1 每一节的结构（固定六件套）

| 小节 | 含义 |
|---|---|
| 🎯 目标 | 这一步做完，你会拥有什么"看得见"的结果 |
| 📚 要学会的概念 | 这一步涉及的 UE 概念，**先看官方文档再动手**（我给了要搜的关键词） |
| 📁 要建的文件 | 具体路径与文件名 |
| ✍️ 你要写的逻辑 | 关键类 / 函数 / 数据流（不写实现） |
| ⚠️ 常见坑 | 我踩过或预判到的坑，提前告诉你，省你 2 小时 |
| ✅ 自测 | 怎么确认这步真的做对了（不是"编译过了"就算对） |

### 0.2 三条铁律（否则后面会返工）

1. **每一步都必须能编译 + 能运行 + 能自测**。不要"一口气写 10 个类再一起编译"——UE 的编译错误会互相掩盖，新手会被淹没。
2. **先跑通链路，再做功能**。例如先让"假密码机"能被按 F（第 9 步），再去实现真实密码机的校准 QTE（第 12 步）。链路不通时做功能，等于在流沙上盖楼。
3. **代码可以少，接口契约不能改**。`design_analysis.md` 里的类名、函数签名、标签名是第 1 轮定下的契约，改了要回文档同步，否则架构会散。

### 0.3 时间预估（按你每天 2~3 小时投入）

| 阶段 | 步骤 | 预估 | 累计 |
|---|---|---|---|
| 阶段 0：工程与习惯 | S01–S04 | 2~3 天 | 3 天 |
| 阶段 1：GAS 地基 | S05–S09 | 1.5~2 周 | 约 2.5 周 |
| 阶段 2：交互系统 | S10–S13 | 1~1.5 周 | 约 4 周 |
| 阶段 3：移动与相机 | S14 | 3~4 天 | 约 4.5 周 |
| 阶段 4：地图 | S15 | 1~1.5 周 | 约 6 周 |
| 阶段 5：交互点玩法 | S16–S18 | 2.5~3 周 | 约 9 周 |
| 阶段 6：角色 | S19–S23 | 3~4 周 | 约 13 周 |
| 阶段 7：AI 与打磨 | S24 | 1.5~2 周 | 约 15 周 |

> 卡住超过 1 天就来找我，不要硬扛——很多坑是 UE 的"知识盲区"，不是你的能力问题。

### 0.4 关于"简历价值"（重要，请先读）

你写这个项目是为了简历，所以**每完成一步，立刻记三样东西**（建议在 `Documents/devlog/` 下按步骤建文件）：

| 记录项 | 例子 |
|---|---|
| **我做了什么**（技术名词要准确） | "用 GAS 的 `UAbilitySet` 数据资产聚合角色的技能/属性/输入，实现角色能力的热插拔" |
| **难点与我的决策** | "破译进度用每 0.1s 离散复制而不是每帧属性复制，带宽从 X 降到 Y" |
| **可量化结果** | "150ms 延迟下进度 5 端误差 < 1%"、"单客户端下行 28KB/s" |

面试官不会问"你会不会写 UE"，会问"**为什么这么设计**"。所以本路线图每一步都带一个 **💼 简历点**，告诉你这一步能写成什么、可能会被追问什么。

---

## 阶段 0：工程与开发习惯（S01–S04）

### S01 ✅ 补齐插件 + 首次重编译

🎯 **目标**：工程能编译、编辑器能打开、9 个插件真正生效。

**先解决两处遗漏**（我核对了你的 `.uproject`）：

| 问题 | 说明 | 处理 |
|---|---|---|
| `StateTree` 未写入 `.uproject` | 它是 `GameplayStateTree` 的依赖，可能只是没显式落盘 | 去插件面板搜 `State Tree`，确认**已勾选**（勾选状态下点一下取消再勾上，然后重启，确保写入 `.uproject`） |
| `GameplayInsights` 未写入 `.uproject` | 同上（State Tree 的依赖） | 搜 `Gameplay Insights`，确认已勾选 |

📚 **要学会的概念**：
- 搜：`Unreal Engine 插件 Plugins 面板`、`Unreal Build Tool UBT`、`Live Coding`
- 关键认知：**.uproject 里的 Plugins 数组 = 事实上的开关**，插件面板只是它的可视化编辑器。

📁 **要建的文件**：无。

✍️ **你要写的逻辑**：无。

⚠️ **常见坑**：
- 编译前**必须完全关闭编辑器**（Live Coding 在编辑器里编译插件代码会失败或半残）。
- 第一次编译会很久（10~30 分钟），别以为卡死了。
- 编译命令（关掉编辑器后在 PowerShell 里跑）：
  ```
  & "G:\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" Identity5Editor Win64 Development -Project="G:\Project\Unreal Projects\Identity5\Identity5.uproject" -WaitMutex
  ```
  （把 `G:\Epic Games\UE_5.8` 换成你实际的引擎路径）

✅ **自测**：
1. 编译输出最后一行是 `Total execution time: ...` 且**没有 `Error`**；
2. 打开编辑器，**编辑 → 插件**，搜索 `Gameplay Abilities`，显示"已启用"；
3. 输出日志（`Saved/Logs/Identity5.log`）里搜 `GameplayAbilities`，能看到模块加载记录。

💼 **简历点**：暂无可写，但这一步的"模块化插件架构"是后面所有描述的地基。

---

### S02 ✅ 建立 Git + LFS + 忽略清单（工程化基本功）

🎯 **目标**：代码可回溯，`.uasset`/`.umap` 能正确版本化，仓库不臃肿。

📚 **要学会的概念**：
- 搜：`Git LFS Unreal Engine`、`Unreal Engine .gitignore recommended`
- 关键认知：UE 的二进制资产（uasset/umap）必须走 LFS，否则仓库会爆；`Binaries/`、`Intermediate/`、`DerivedDataCache/`、`Saved/` 不该进仓库。

📁 **要建的文件**：
- `.gitignore`
- `.gitattributes`

✍️ **你要写的逻辑**：
- `.gitattributes`：把 `*.uasset`、`*.umap`、`*.fbx`、`*.wav`、`*.png`、`*.tga` 等指到 LFS；
- `.gitignore`：忽略 `Binaries/`、`Intermediate/`、`DerivedDataCache/`、`Saved/`、`.vs/`、`*.sln`（可选，有人喜欢提交 sln）、`*.VC.db`。

⚠️ **常见坑**：
- 先 `git init` 再写 `.gitattributes` **之前**不要提交任何 uasset（否则历史里已经塞进去了，很难清）。
- LFS 要 `git lfs install` 一次（每台机器一次）。

✅ **自测**：`git check-attr filter -- Content/xxx.uasset` 输出包含 `lfs`。

💼 **简历点**："为大体积 UE 工程配置 Git LFS 与资产版本化策略"（工程能力，面试官会当成"你是否团队可用"的信号）。

---

### S03 ✅ 项目配置：默认地图 / GameMode / 网络参数

> 📄 **详细实施步骤见 [steps/S03-project-config.md](steps/S03-project-config.md)**（含 ini 分层机制讲解、两层 GameMode 设计原因、逐项自测与常见坑）。

🎯 **目标**：编辑器启动就有正确的默认地图与 GameMode 骨架，网络参数就位。

📚 **要学会的概念**：
- 搜：`DefaultEngine.ini`、`GameMapsSettings`、`AGameModeBase`、`GlobalDefaultGameMode`
- 关键认知：`Config/Default*.ini` 是项目级配置，优先级高于引擎 `Base*.ini`。

📁 **要修改的文件**：`Config/DefaultEngine.ini`、`Config/DefaultGame.ini`

✍️ **你要写的逻辑**（参考 `design_analysis.md` 第 2 轮 1.2 节与 9.2 节）：
- `[/Script/EngineSettings.GameMapsSettings]`：`EditorStartupMap`、`GameDefaultMap`、`GlobalDefaultGameMode`
- `[SystemSettings]`：`net.UseAdaptiveNetUpdateFrequency=1`、`net.MaxNetUpdateFrequency=60`
- 地图还没建，先把路径占好（`/Game/IDF/Maps/L_MilitaryFactory`），后面建了就不改配置了。

⚠️ **常见坑**：
- 地图不存在时写进配置不会报错，但会让你误以为"配置没生效"——先建个空关卡放进去。
- `GlobalDefaultGameMode` 写的是 **C++ 类路径**（`/Script/模块名.类名`，去掉类名前缀 `A`），不是蓝图路径。

✅ **自测**：启动编辑器，直接按 PIE，`World Settings` 面板里 `GameMode Override` 显示你配置的类。

💼 **简历点**："规范化项目配置与默认流程装配"。

---

### S04 ✅ 日志与调试基础设施（新手最容易忽略、老手最先做的事）

> 📄 **详细实施步骤见 [steps/S04-logging-debug.md](steps/S04-logging-debug.md)**（含日志级别/编译期上限/ODR 问题讲解、CVar 与 Console Command 的区别、10 项自测与常见坑）。

🎯 **目标**：有自己模块的日志类别、能分级输出、屏幕调试信息可控开关。

📚 **要学会的概念**：
- 搜：`DECLARE_LOG_CATEGORY_EXTERN`、`UE_LOG 宏`、`GEngine->AddOnScreenDebugMessage`、`FAutoConsoleCommand`
- 关键认知：UE 默认日志刷屏严重，**自定义 LogCategory 是定位问题的唯一高效手段**。

📁 **要建/改的文件**：
- `Source/Identity5/Public/Logging/Identity5Log.h` / `Private/Logging/Identity5Log.cpp`（主模块只放 `LogIdentity5`）
- 系统类日志归各自插件：`Plugins/GameFeatures/IDFCore/Source/IDFCore/Public/Logging/IDFCoreLog.h`（S05 建）、`.../IDMMatch/.../Logging/IDMMatchLog.h`（S10 建）

✍️ **你要写的逻辑**：
1. 主模块只声明/定义 `LogIdentity5`（`DECLARE_LOG_CATEGORY_EXTERN` + `DEFINE_LOG_CATEGORY` 配对）；
2. 其余系统类日志（`LogIDFNet` / `LogIDFAbility` / `LogIDFAttribute` / `LogIDFInteraction`）**在 S05 建 `IDFCore` 时直接建在插件里**，不要先建在主模块再迁；
3. 写一个控制台命令 `IDF.Debug.Hello 1`（用 `FAutoConsoleCommand`），验证命令系统可用；
4. 在 `DefaultEngine.ini` 里设置默认日志级别（**类名不变，与定义位置无关**）：
   ```ini
   [Core.Log]
   LogIdentity5=Log
   LogIDFInteraction=Verbose
   LogIDFNet=Log
   ```

⚠️ **常见坑**：
- 日志类别名重复定义会链接失败（`duplicate symbol`）；声明在头、定义只能一处；
- 日志类别写进插件后，插件不要再反向依赖主模块取类别（会破坏 S20 的解耦验收）；
- `CompileTimeVerbosity` 写小会把 `Verbose` 日志从二进制里删掉，之后 ini 怎么调都看不到；
- 控制台命令的参数解析：`FAutoConsoleCommandWithWorldAndArgs` 比手写解析省事。

✅ **自测**：游戏里按 `~` 输入 `IDF.Debug.Hello 1`，屏幕出现文字；日志文件里能看到 `LogIdentity5` 的输出。

💼 **简历点**："按归属模块划分日志类别（平台类归主模块、系统类归所属插件），区分运行时级别与编译期上限，并提供运行时调试开关"。

---

## 阶段 1：GAS 地基（S05–S09）

> 这五个步骤是整个项目**技术含量最高**的部分，也是简历上最值钱的部分。慢一点没关系。

### S05 ✅ 最小 GAS 骨架：ASC 自定义类 + 属性集

> 📄 **详细实施步骤见 [steps/S05-gas-skeleton.md](steps/S05-gas-skeleton.md)**（含 Meta 属性不复制的原因、`REPNOTIFY_Always` 的必要性、UE5.8 与教程不一致的 3 处 API、客户端属性为 0 的排查表）。

🎯 **目标**：**先新建 `IDFCore` GameFeature 插件**，然后在插件里实现 `UIDFAbilitySystemComponent` 与 `UIDFHealthSet`，编译通过、属性在客户端能正确复制。

> ⚠️ **S05 起 `IDFCore` 插件就必须存在**（原计划把它放在 S10，已修正）。原因：GAS 骨架是"GAS 胶水"，按设计文档第 2 轮 2.1 节属于 `IDFCore`；而 C++ 类**跨模块搬迁不是移动文件**——S06 一旦创建引用这些类的资产（`DA_*`/`GE_*`/蓝图父类），跨模块搬迁会导致资产断链、需要逐个人工重定向。**S05 是零代价窗口（只有 4 个源文件、零资产引用）**。

📚 **要学会的概念**（**必须读官方文档**）：
- 搜：`Gameplay Ability System Unreal`、`AbilitySystemComponent`、`AttributeSet`、`ATTRIBUTE_ACCESSORS_BASIC`、`GetLifetimeReplicatedProps`、`GAMEPLAYATTRIBUTE_REPNOTIFY`、`Game Feature Plugin`
- 关键认知：
  - `UAttributeSet` 的属性要**手工写复制**（宏帮你做了大部分）；
  - 生成访问器的宏在 5.8 里叫 **`ATTRIBUTE_ACCESSORS_BASIC`**（`ATTRIBUTE_ACCESSORS` 只存在于注释示例里）；
  - `PreAttributeChange`（夹取用）和 `PostGameplayEffectExecute`（结算用）职责不同；
  - **插件模块的导出宏是 `IDFCORE_API`**，不是主模块的 `IDENTITY5_API`。

📁 **要建的文件**（用编辑器的 **Add → Game Feature** 创建插件 `IDFCore`，**不要手写 `.uplugin`**）：

向导会自动生成这些（**不需要你创建**，注意入口文件叫 `IDFCoreModule.*`，不存在 `IDFCore.h/.cpp`）：
```
Plugins/GameFeatures/IDFCore/IDFCore.uplugin
Plugins/GameFeatures/IDFCore/Content/IDFCore.uasset          （GameFeature 数据资产）
Plugins/GameFeatures/IDFCore/Resources/Icon128.png
Plugins/GameFeatures/IDFCore/Source/IDFCore/IDFCore.Build.cs
Plugins/GameFeatures/IDFCore/Source/IDFCore/Public/IDFCoreModule.h
Plugins/GameFeatures/IDFCore/Source/IDFCore/Private/IDFCoreModule.cpp
```

本节你要**新增**的（全部放在 `Source/IDFCore/` 的子目录里）：
```
Public/Logging/IDFCoreLog.h
Private/Logging/IDFCoreLog.cpp
Public/AbilitySystem/IDFAbilitySystemComponent.h
Private/AbilitySystem/IDFAbilitySystemComponent.cpp
Public/AbilitySystem/IDFHealthSet.h
Private/AbilitySystem/IDFHealthSet.cpp
Public/Debug/IDFGasTestActor.h
Private/Debug/IDFGasTestActor.cpp
```

同时**修改**主模块的日志（类别迁走的连带改动，见详细文档 -1.6）：
```
Source/Identity5/Public/Logging/Identity5Log.h    （删掉 4 个类别声明，只留 LogIdentity5）
Source/Identity5/Private/Logging/Identity5Log.cpp （同步删掉 4 个 DEFINE_LOG_CATEGORY）
```

✍️ **你要写的逻辑**：
1. 先**核对**（不是重写）向导生成的 `IDFCore.Build.cs`：`GameplayAbilities` / `GameplayTags` / `GameplayTasks` 应在 `PublicDependencyModuleNames` 里；
2. `UIDFHealthSet`：`Health`、`MaxHealth`、`Damage`(Meta)、`Healing`(Meta) 四个属性；
3. `PreAttributeChange`：把 `Health` clamp 到 `[0, MaxHealth]`；
4. `PostGameplayEffectExecute`：**注意 5.8 的签名是 `const FGameplayEffectModCallbackData&`**（我在引擎源码里核实过，网上大量教程写的是非 const 引用，是旧版本）；
5. `UIDFAbilitySystemComponent`：先只加 `PrintDebugInfo()` 和日志，不急着写输入转发。

⚠️ **常见坑**：
- **`IDENTITY5_API` 必须改成 `IDFCORE_API`**（类换模块了），否则链接报 `unresolved external symbol`；
- 插件没写进 `.uproject` 的 `Plugins` 数组 → UBT 不编译它，症状是"代码像没生效"；
- **`.uplugin` 的 `"Plugins"` 数组里只能写真实插件**（如 `GameplayAbilities`）；把引擎模块（如 `GameplayTags`）写进去会**直接构建失败**：`Unable to find plugin 'GameplayTags'`。模块依赖要写在 `Build.cs` 的 `DependencyModuleNames` 里；
- **插件描述符里没有 `"Type"` 字段**：`"Type": "Runtime"` 是 `Modules[]` 里每个条目的**模块**类型，别误当成"插件类型"去改成 `"GameFeature"`（插件类型由位置推断，GameFeature 身份由 `Content/IDFCore.uasset` + `BuiltInInitialFeatureState` 决定）；
- 插件的 `.uplugin` 里 `LoadingPhase` 用 `Default`，不要用 `PreDefault`（早于 Engine 反射注册，会"找不到类"）；
- 新建/重命名插件后**必须重新生成工程文件**（右键 `.uproject` → Generate Visual Studio project files），否则 Rider/VS 不会索引插件代码；改过模块名后要清掉 `Plugins/GameFeatures/<插件>/Binaries` 与 `Intermediate` 再重编；
- `FGameplayEffectModCallbackData` 在 5.8 里由 `GameplayEffectExtension.h` 提供（该文件现在只剩结构体定义，注释里写着 legacy），要 include 它；
- `PostGameplayEffectExecute` 的签名是 `const FGameplayEffectModCallbackData&`（漏 `const` 会报"未覆盖基类虚函数"）；
- 生成访问器的宏是 **`ATTRIBUTE_ACCESSORS_BASIC`**，`ATTRIBUTE_ACCESSORS` 只存在于注释里；
- 属性没复制：忘了在 `GetLifetimeReplicatedProps` 里加 `DOREPLIFETIME_CONDITION_NOTIFY` + `REPNOTIFY_Always`；
- `GAMEPLAYATTRIBUTE_REPNOTIFY` 的参数顺序写错（是 `ClassName, PropertyName, OldValue`）；
- **Meta 属性（Damage/Healing）不应复制**（它们只在服务器瞬间存在）——这是新手最常犯的带宽错误；
- 插件的 `Binaries/`/`Intermediate/` 需要单独 ignore（工程根 `/Binaries/` 是锚定规则，不匹配子目录）。

✅ **自测**：插件在插件面板显示"已启用"；编译通过；**用 Listen Server + 4 个客户端验证属性在客户端也能读到 `100.0/100.0`**（这是本节最关键的一项，见详细文档的验证项 6）。

> 📄 验证前需要一张地图：先按 **[steps/S05-verify-level.md](steps/S05-verify-level.md)** 建 `Content/IDF/Maps/L_MilitaryFactory`（Basic 模板 + 地面 + PlayerStart），再把 `AIDFGasTestActor` 拖进去跑 PIE。这张图**不是一次性的**——S15 的军工厂灰盒会在同一张图上继续做。

💼 **简历点**："用 GameFeature 插件划分框架层（IDFCore）与启动装配（主模块），保证依赖单向；基于 GAS 实现自定义 AbilitySystemComponent 与 AttributeSet，区分持久属性与 Meta 属性以控制复制开销"。

---

### S06 ✅ 数据资产：AbilitySet + InputConfig + 模块内 Tag 注册

> 📄 **详细实施步骤见 [steps/S06-ability-set-input-config.md](steps/S06-ability-set-input-config.md)**（含：原生标签的"头声明/源定义"分工与宏内 `static_assert` 拦截、AbilitySet 装配顺序为何不能颠倒（属性集→GE→能力）、`FGameplayAbilitySpec` 的 `InputID` 已在 5.5 废弃改用标签、平行数组的结构性隐患与 USTRUCT 替代方案、`UPrimaryDataAsset` vs `UDataAsset` 的取舍）。
>
> ⚠️ **本节需要先改 `IDFCore.Build.cs`**：把 **`EnhancedInput`** 加进 `PublicDependencyModuleNames`（`UIDFInputConfig` 要用 `UInputMappingContext` / `UInputAction`）。

🎯 **目标**：有一份"角色能力装配表"（`UIDFAbilitySet`）和"输入映射表"（`UIDFInputConfig`），以及 C++ 原生的 GameplayTag。

📚 **要学会的概念**：
- 搜：`Primary Data Asset`、`FGameplayAbilitySpecHandle`、`GiveAbility`、`FNativeGameplayTag`、`UE_DEFINE_GAMEPLAY_TAG`
- 关键认知：
  - **原生标签**（C++ 声明）比 ini 标签安全：编译期检查、无字符串查找、不依赖 ini 加载顺序；
  - `UAbilitySet` 模式（Lyra 的做法）解决"角色能力散落各处、难以维护"的问题。

📁 **要建的文件**：
```
Source/IDFCore/Public/IDFGameplayTags.h/.cpp          （全量标签声明 + 定义）
Source/IDFCore/Public/AbilitySystem/IDFAbilitySet.h/.cpp
Source/IDFCore/Public/AbilitySystem/IDFInputConfig.h/.cpp
Config/DefaultGameplayTags.ini                        （仅用于编辑器可见性的兜底，可选）
```

✍️ **你要写的逻辑**：
1. `IDFGameplayTags.h`：用 `UE_DECLARE_GAMEPLAY_TAG_EXTERN` 声明标签（清单见设计文档第 2 轮 5.2 节），`.cpp` 里用 `UE_DEFINE_GAMEPLAY_TAG` 定义；
2. `UIDFAbilitySet::GiveToAbilitySystem()`：遍历三组数组，分别 `GiveAbility` / `ApplyGameplayEffectToSelf` / `AddAttributeSetSubobject`，并输出授予的 `SpecHandle` 数组；
3. `UIDFInputConfig`：定义原生输入（移动/视角/冲刺/蹲）与能力输入（交互/技能）两组资产字段；
4. 先做一个空能力 `GA_IDF_Test`（激活时只打日志），用于验证装配链路。

⚠️ **常见坑**：
- `UE_DECLARE_GAMEPLAY_TAG_EXTERN` + `UE_DEFINE_GAMEPLAY_TAG` 必须成对，且 `UE_DEFINE` 只能出现在一个 `.cpp` 里（重复定义 → 链接错误）；
- 标签字符串不要手写错（`State.Survivor.Injured` ≠ `State.Survivor.Injuried`）——**只写一次，其他全用常量**；
- `GiveAbility` 必须在 ASC 已 `InitAbilityActorInfo` 之后调用。

✅ **自测**：控制台 `IDF.Debug.Tags.Validate`（S05 时先不做校验，这里补上）无报错；编辑器里能创建 `DA_IDF_AbilitySet_Placeholder` 数据资产。

💼 **简历点**："用 PrimaryDataAsset 实现能力装配表（AbilitySet），角色能力与属性可通过数据资产热插拔，新增角色不改框架代码"。

---

### S07 ✅ 角色基类：ASC 初始化与网络双端时序

> 📄 **详细实施步骤见 [steps/S07-character-base.md](steps/S07-character-base.md)**（含：双端两条路径为何必须收敛到同一幂等函数、初始化内部六步顺序各自颠倒的后果、`NetUpdateFrequency` 在 5.5 已废弃改用 Setter、`Abstract` 角色类不能直接做 `DefaultPawnClass` 的解决办法、客户端初始化失败的排查表）。
>
> ⚠️ **两处设计文档假设需要修正（我核实过引擎源码）**：
> 1. **`UGameplayMessageSubsystem` 不在引擎里**——它是 Lyra 样例工程自带的。本节先改用 `BlueprintAssignable` 委托，等 S13 真有"跨系统按标签广播"的需求时再自研。
> 2. **`InitState.Spawned` / `DataInitialized` / `GameplayReady` 这套标签引擎也不提供**——`UGameFrameworkComponentManager::RegisterInitState()` 只维护顺序数组，标签名是使用方的约定。**标签由我们自己在 `IDFGameplayTags.h` 里定义**（沿用 Lyra 命名便于对照资料）。

🎯 **目标**：`AIDFCharacterBase` 在服务器和客户端都能正确初始化 ASC，两个端都能读到 `State.Ready` 标签。

📚 **要学会的概念**（**这一步的核心，务必搞懂**）：
- 搜：`IAbilitySystemInterface`、`InitAbilityActorInfo`、`PossessedBy`、`OnRep_PlayerState`、`SetupPlayerInputComponent`、`bReplicates`、`SetReplicateMovement`
- 关键认知：
  - **服务器**走 `PossessedBy`，**客户端**走 `OnRep_PlayerState`（或 `OnRep_Controller`），这是 GAS 最经典的"双端初始化"问题；
  - 客户端初始化必须**幂等**（RepNotify 可能多次触发），用一个 `bool bInitialized` 挡住；
  - 初始化顺序：`InitAbilityActorInfo` → 加 AttributeSet → 授予能力 → 加 `State.Ready` 标签 → 广播事件。**顺序错了会有各种"静默失败"**。

📁 **要建的文件**：
```
Source/IDFCore/Public/Characters/IDFCharacterBase.h/.cpp
Source/IDFCore/Public/Characters/IDFSurvivorCharacter.h/.cpp   （本轮占位，抽象类）
Source/IDFCore/Public/Characters/IDFHunterCharacter.h/.cpp     （本轮占位，抽象类）
```

✍️ **你要写的逻辑**：
1. 构造函数：`bReplicates = true`、`NetUpdateFrequency = 30`、`MinNetUpdateFrequency = 10`、`bUseControllerRotationYaw = false`、`bOrientRotationToMovement = true`、禁用跳跃；
2. `GetAbilitySystemComponent()`：返回自己的 ASC（并做 PlayerState 兜底查询，见设计文档第 2 轮 3.1 节）；
3. `InitializeAbilitySystem()`：按上面的顺序实现，加 `bInitialized` 守卫；
4. `PossessedBy` / `OnRep_PlayerState` 分别调用它；
5. 在客户端初始化完成后，用 GameplayMessage 广播 `Event.Character.Ready`。

⚠️ **常见坑**（这一节的坑最多，慢慢看）：
- **客户端 ASC 的 AvatarActor 是 PlayerState 还是 Character？** 第 1 轮决定 ASC 挂 Character，所以 `InitAbilityActorInfo(this, this)`；
- `OnRep_PlayerState` 在**服务器**也会调用一次——别在里面重复初始化；
- 只写 `PossessedBy` 不写 `OnRep_PlayerState` → 客户端上技能栏是空的（经典症状）；
- 忘了 `SetReplicateMovement(true)`（`ACharacter` 默认已开，但换基类容易丢）；
- `MaxWalkSpeed` 在客户端改没用——速度是移动组件在服务器算的（后面 S14 会细讲）。

✅ **自测**：
1. PIE 双窗口，服务器窗口 `showdebug abilitysystem` 能看到属性；
2. **客户端窗口**也能看到（这一条最关键，测的就是双端初始化）；
3. 屏幕上能打印出 `State.Ready` 标签。

💼 **简历点**："解决 GAS 在服务器/客户端的双端初始化时序问题（PossessedBy vs OnRep_PlayerState 的幂等初始化）"——**这是面试高频考点，一定要写**。

---

### S08 ✅ 玩家状态 / 对局状态 / GameMode / PlayerController

🎯 **目标**：5 个 PIE 窗口能进来、能生成角色、阵营分配正确、`MatchPhase` 能在服务器改并同步到所有客户端。

📚 **要学会的概念**：
- 搜：`AGameModeBase PostLogin`、`APlayerState`、`AGameStateBase`、`ReplicatedUsing`、`OnRep`、`ChoosePlayerStart`、`GetServerWorldTimeSeconds`
- 关键认知：
  - **只有服务器能写复制变量**，客户端只能读（写入会被下一次复制覆盖）；
  - `GameState` 是"所有客户端都需要知道的对局数据"的家（阶段、计数、服务器时间）；
  - `PlayerState` 是"每个玩家个人的数据"的家（阵营、角色、准备状态）。

📁 **要建的文件**（`Plugins/GameFeatures/IDMMatch/` 下）：
```
IDMMatch.uplugin / IDMMatch.Build.cs
Source/IDMMatch/Public/IDMMatch.h/.cpp
Source/IDMMatch/Public/IDMGameMode.h/.cpp
Source/IDMMatch/Public/IDMGameState.h/.cpp
Source/IDMMatch/Public/IDMPlayerState.h/.cpp
Source/IDMMatch/Public/IDMPlayerController.h/.cpp
Source/IDMMatch/Public/Data/IDMMatchRuleSet.h/.cpp      （数据资产，先只填几个字段）
```

✍️ **你要写的逻辑**：
1. `AIDMPlayerState`：`Team`、`CharacterTag`、`bReady` 三个复制字段 + `SetTeam()` 等**服务器专用** setter（用 `ensure(HasAuthority())` 保护）；
2. `AIDMGameState`：`MatchPhase`（枚举）、`DecodedCount`、`ServerTime` + 各自的 `RepNotify`；`Tick` 里每 1 秒更新一次 `ServerTime`；
3. `AIDMGameMode::PostLogin`：按 `PlayerArray` 的索引分配阵营（0 号给监管者）+ `SetTeam`；
4. `AIDMPlayerController`：`BeginPlay` 里把自己注册到 UI/输入（本轮先只打日志）；
5. `AIDMGameState::SetMatchPhase()` 只能服务器调用 + 加 `UE_LOG`。

⚠️ **常见坑**：
- 在客户端调 `SetTeam()` → 看似生效，一秒后被服务器复制值覆盖（症状："UI 闪一下变回去"）；
- 枚举复制要用 `UPROPERTY(ReplicatedUsing=OnRep_X)`，枚举**不需要** `DOREPLIFETIME` 之外的额外处理，但 `OnRep` 函数名必须完全匹配；
- `PlayerState` 的复制：`bReplicates = true` 在 `APlayerState` 里默认已开，但 `PlayerStateClass` 要在 `GameMode` 构造函数里指定；
- `PostLogin` 里 `PlayerState` 可能还没完全初始化，索引分配建议放在 `HandleStartingNewPlayer` 或第一帧 `Tick` 里。

✅ **自测**：
1. 5 窗口 PIE，每个窗口屏幕左上角打印自己的阵营；
2. 服务器窗口执行 `IDF.Match.ForcePhase Playing`，**5 个窗口**的显示都变化（验证 GameState 复制）；
3. 客户端窗口执行同一命令 → 无效（验证"只有服务器能改"）。

💼 **简历点**："设计服务器的权威对局状态机（Lobby→Warmup→Playing），用 GameState/PlayerState 分层复制对局数据与玩家数据"。

---

### S09 ✅ 里程碑验收：5 窗口 + 移动同步 + GAS 生效

🎯 **目标**：达成设计文档的 **AC-R2-2 / AC-R2-3 / AC-R2-4 / AC-R2-5**。

📚 **要学会的概念**：
- 搜：`Play As Listen Server`、`Run Under One Process`、`stat net`、`net PktLag`、`Network Emulation`
- 关键认知：PIE 的多客户端不是"假网络"，它走真实 UDP 复制；`RunUnderOneProcess=False` 更接近真实联机。

📁 **要修改的文件**：`Config/DefaultEditorPerProjectUserSettings.ini`（配置见 `testing_pie.md` 第 1.1 节）

✍️ **你要写的逻辑**：
1. 建一张最简单的关卡（`L_IDF_Test`，放个地板 + `PlayerStart` ×5）；
2. 做完 `testing_pie.md` 的 TC-01（移动同步）与第 2.2 节自检；
3. 用 `stat net` 记录一下当前带宽数字（**这是你简历里的量化数据，现在就开始攒**）。

⚠️ **常见坑**：
- 5 个窗口全黑/只有一个能连：看 `testing_pie.md` 第 4 章（端口冲突 / GPU 压力）；
- 移动同步但**动画不同步**（TPS 动画在客户端抽搐）：这是后面 S14 的 `CharacterMovement` 调参问题，先记下来；
- 别忘了 `PlayerStart` 数量要 ≥ 5，否则玩家会挤在一起生成。

✅ **自测**：`testing_pie.md` 的 TC-01 通过；带宽截图存档。

💼 **简历点**："搭建 5 客户端 Listen Server 联调环境，验证移动复制与阵营分配"。

---

## 阶段 2：交互系统（S10–S13）

> 这是本项目的**架构亮点**：把"板/窗/机/椅/门"统一抽象成一套交互协议。做好了，后面 7 种交互点就是"填表格"。

### S10 ✅ 交互契约：接口 + 数据结构

> 注：`IDFCore` 插件在 S05 就已创建，本节是**往已有插件里加文件**（不再是"新建插件"）。新增文件用 `IDFCORE_API`。

🎯 **目标**：`IIDFInteractableTarget` 接口与 `FIDFInteractionOption` / `FIDFInteractionRequest` 结构体编译通过。

📚 **要学会的概念**：
- 搜：`UINTERFACE` 与 `IInterface` 的区别、`BlueprintNativeEvent` vs 纯虚函数、`USTRUCT` 在 RPC 中的使用、`TWeakObjectPtr`
- 关键认知：
  - `UINTERFACE` 只是给反射用的壳，真正的接口是 `I` 开头的那个类；
  - 接口函数若要在蓝图里实现，要用 `BlueprintNativeEvent`（会生成 `_Implementation`）；
  - **RPC 参数必须是 USTRUCT/UObject/基础类型**，不能传裸引用。

📁 **要建的文件**：
```
Source/IDFCore/Public/Interaction/IDFInteractionTypes.h
Source/IDFCore/Public/Interaction/IDFInteractableTarget.h/.cpp
```
（结构体定义见 `design_analysis.md` 第 2 轮 4.1 / 4.2 节，**这部分契约照抄，不要自创字段**）

✍️ **你要写的逻辑**：
1. 三个枚举：`EIDFInteractionType`（Hold/Press/Toggle）、`EIDFInteractionAvailability`（Available/Unavailable/Hidden）；
2. `FIDFInteractionOption`：**重点想清楚每个字段为什么存在**（例如 `InteractionTransform` 是给 MotionWarping 对齐站位的，`UnavailableReason` 是给 UI 显示"已破译"的）；
3. 接口方法：`GatherInteractionOptions`（纯虚，必须实现）、`OnInteractionBegin/Update/End`（空实现）、`CanInteract`（默认 true）。

⚠️ **常见坑**：
- 在头文件里 include 另一个头文件造成循环依赖 → 用前向声明 + 在 `.cpp` 里 include；
- `USTRUCT` 忘记 `GENERATED_BODY()`；
- 接口里的 `TObjectPtr`/`TWeakObjectPtr` 成员在复制时的坑（第 11 步会遇到）。

✅ **自测**：编译通过；建一个空 Actor 实现该接口，能在编辑器里对接口函数打断点。

💼 **简历点**："设计统一的交互抽象层（Interface + Option/Request 数据结构），支撑 7 类交互点的提示、校验与表现复用"。

---

### S11 ✅ 交互组件（上）：客户端扫描与 UI 提示

🎯 **目标**：客户端靠近交互点能在屏幕上看到"按住 F 破译"，离开消失。**先不接服务器**。

📚 **要学会的概念**：
- 搜：`SweepMultiByObjectType`、`FCollisionQueryParams`、`ECC_GameTraceChannel`（自定义碰撞通道）、`LineOfSightTo`
- 关键认知：交互检测用 **Sweep（有体积）** 而不是 Overlap，是为了避免"隔着薄墙提示隔壁的密码机"。

📁 **要建的文件**：
```
Source/IDFCore/Public/Interaction/IDFInteractionComponent.h/.cpp
```

✍️ **你要写的逻辑**：
1. 在 `DefaultEngine.ini` 里新增碰撞通道 `IDF_Interactable`（`Channel` 配置，`DefaultEngine.ini` 的 `[/Script/Engine.CollisionProfile]` 段）；
2. `TickComponent`：每 0.1s（用累计时间，不要每帧）用 `SweepMultiByObjectType` 收集候选；
3. 过滤：实现接口 + `IsInteractableVisibleFor()`；
4. 选择最优：距离 + 视线 + `Priority`；
5. **只在结果变化时**回调 UI（`OnInteractionUIChanged`），避免每帧刷新 UI 抖动；
6. 屏幕调试打印当前选中的选项（先不做 UMG）。

⚠️ **常见坑**：
- `SweepMulti` 的 `FCollisionObjectQueryParams` 要同时配 `ObjectTypes` 和通道，配错了永远返回空；
- 忘记忽略自己（`AddIgnoredActor(GetOwner())`）；
- 每帧 Sweep + 每帧 UI 刷新 → 帧率杀手；
- `TWeakObjectPtr` 用之前要 `IsValid()`。

✅ **自测**：放 3 个测试交互点在场景里，走近逐个切换，屏幕文字正确变化；站在墙两侧，只有正确的一侧显示提示。

💼 **简历点**："实现客户端交互检测（Sweep + 视线 + 优先级），采用变化驱动刷新避免 UI 抖动"。

---

### S12 ✅ 交互组件（下）：服务器校验与预测

🎯 **目标**：交互请求走服务器、服务器做 7 项校验、拒绝时客户端停止本地表现。**这一节是网络同步的核心。**

📚 **要学会的概念**（重点）：
- 搜：`Server RPC WithValidation`、`UFUNCTION(Server, Reliable)`、`NetMulticast`、`Client RPC`、`客户端预测`、`SequenceId 防重放`
- 关键认知：
  - **RPC 的 `_Validate` 只做参数合法性**（空指针/非法标签），**业务校验必须在 `_Implementation` 里做**（因为 `_Validate` 失败会踢人，而且它拿不到游戏状态）；
  - 距离校验要给**延迟容差**（玩家在 150ms 延迟下，客户端看到的位置和服务器差 1~2 个身位）；
  - `NetMulticast, Unreliable` 用于表现广播：丢了也能靠状态复制兜底。

📁 **要修改的文件**：`IDFInteractionComponent.h/.cpp`

✍️ **你要写的逻辑**：
1. `OnInteractPressed/Released`：本地先起表现（记录 `PendingSequenceId`），再发 `Server_BeginInteraction`；
2. `Server_BeginInteraction_Implementation`：重新 `GatherInteractionOptions` → 距离/朝向/状态/`CanInteract` 校验 → 通过则调目标的 `OnInteractionBegin` + `Multicast_InteractionStarted`；
3. 校验失败 → `Client_InteractionRejected(SequenceId, Reason)`，客户端清掉本地表现；
4. 交互中锁定 `ActiveTarget`，`OnInteractReleased` 时 `Server_EndInteraction`；
5. 写一个调试命令 `IDF.Debug.Interact.ForceRequest <模式>`：模式 0 = 超距离强制请求，模式 1 = 非法标签请求（**故意作弊，用来验证服务器拦得住**）。

⚠️ **常见坑**：
- `Server_` RPC 在**单机/Standalone** 下会直接本地执行（容易误判"服务器校验没生效"）→ 必须在 Listen Server + 客户端窗口里测；
- `WithValidation` 的 `_Validate` 返回 `false` 会导致**断线**，别把业务校验写进去；
- 传 `FTransform` 做距离校验时注意：客户端发的是**它自己认为的位置**，服务器要用**服务器上的真实位置**，客户端那个只作为"客户端视角"参考；
- 多层 RPC 嵌套（Client → Server → Multicast）要注意执行顺序。

✅ **自测**（这就是 `testing_pie.md` 的 TC-04）：
1. 正常交互 → 5 个窗口都能看到表现；
2. 强制超距离请求 → 服务器日志有拒绝记录、客户端表现停止、**没有假进度**；
3. 150ms 延迟下反复靠近/远离 → 交互能稳定建立。

💼 **简历点**："实现服务器权威的交互校验（距离/朝向/状态/冷却四重校验 + 延迟容差），客户端预测表现、服务器裁定并回执"——**这是简历里最有分量的一条之一**。

---

### S13 ✅ 假密码机 + 进度同步

🎯 **目标**：一台能被"按住 F 破译"的密码机，进度在 5 个客户端一致（误差 < 1%），达成 AC-R2-6 / AC-R2-8。

📚 **要学会的概念**：
- 搜：`UStateTreeComponent`（GameplayStateTree 插件）、`State Tree 资产创建`、`ReplicatedUsing`、`SetNetUpdateFrequency`、`ForceNetUpdate`
- 关键认知：
  - **进度不要每帧复制**：每 0.1s 或变化超过 0.5% 才复制（设计文档 ADR-7）；
  - 客户端进度条要**平滑插值**到服务器值，不能硬跳（硬跳看起来像卡顿）。

📁 **要建的文件**：
```
Source/IDFCore/Public/Interaction/IDFInteractableStateComponent.h/.cpp
Source/IDFCore/Public/Debug/IDFDebugMachineActor.h/.cpp
```

✍️ **你要写的逻辑**：
1. `UIDFInteractableStateComponent`：`Progress01`（复制）、`bCompleted`（复制+RepNotify）、`StateTag`（复制+RepNotify）、`Interactors` 数组（**仅服务器**）；
2. 服务器 `Tick`：`Progress01 += Rate * DeltaTime`，用"累计 0.1s 才 `MarkItemDirty`"的方式控制复制频率；
3. `AIDFDebugMachineActor`：实现交互接口，选项为 `Interact.Machine.Decode`（Hold 型）；完成时广播 `Event.Machine.Decoded`；
4. 客户端用一个简单的屏幕进度条（`AddOnScreenDebugMessage` 或最简 UMG）；
5. **可选进阶**：把状态机下沉到 `UStateTreeComponent` + StateTree 资产（设计文档第 2 轮 4.5 节），这是简历加分项。

⚠️ **常见坑**：
- `ReplicatedUsing` 的 `OnRep` 在**服务器不会调用**（改了值要自己手动刷新一次 UI）；
- 多个人同时破译时，`Interactors` 数组的增删要在服务器做，客户端不需要知道；
- 进度用 `float` 累加会有精度漂移，建议用整数计数 + 除法（例如累计 0~1000 的 int32）；
- 别用 `DeltaTime` 累加做**长周期计时**（椅子 30 秒、大门 20 秒）——用服务器时间戳算差（设计文档 ADR-5）。

✅ **自测**：5 窗口同时按住 F，5 个屏幕的进度数字一致（录屏对比）；松开后进度停住；完成时所有窗口看到"已破译"。

💼 **简历点**："破译进度采用离散增量复制（0.1s/0.5% 粒度）+ 客户端平滑插值，在 5 端保持 <1% 误差并降低约 20 倍带宽"（**带上你自己测出的真实数字**）。

---

## 阶段 3：移动与相机（S14）

### S14 ✅ 第三人称移动手感 + 相机

🎯 **目标**：移动跟手、相机不穿墙、蹲行可用；监管者与求生者共用框架但参数不同（D3）。

📚 **要学会的概念**：
- 搜：`CharacterMovementComponent` 参数含义（`MaxAcceleration`/`BrakingDecelerationWalking`/`GroundFriction`/`RotationRate`）、`SpringArm` 的 `bDoCollisionTest`/`CameraLagSpeed`、`bOrientRotationToMovement`
- 关键认知：
  - **手感 = 加速度 + 刹车 + 转向速率** 的组合，不是单一速度值；
  - 相机碰撞用 SpringArm 的碰撞检测 + 探针通道，比自己写射线省事；
  - 速度最终由**服务器**的移动组件决定，客户端只做预测。

📁 **要修改的文件**：`IDFCharacterBase.h/.cpp`（移动参数）、新增相机组件（可用蓝图 `BP_IDFCharacterBase` 组装）

✍️ **你要写的逻辑**：
1. 在构造函数里设置设计文档第 2 轮 13.1 节的移动参数，并**每个参数都试着改一改、感受差异**（这一步的价值在于建立手感直觉）；
2. 组装相机：`USpringArmComponent` + `UCameraComponent`，设置臂长/臂高/FOV/延迟（13.2 节表格）；
3. 输入：`IA_Move` / `IA_Look` / `IA_Sprint` / `IA_Crouch` 绑定到角色；
4. 蹲行用 `Crouch()`/`UnCrouch()` + `MaxWalkSpeedCrouched`；
5. 把移动速度改成从 `MoveSpeedMultiplier` 属性读取（为 M5 的加速/减速状态铺路）。

⚠️ **常见坑**：
- 相机穿过角色自己的身体 → 设置 SpringArm 的 `ProbeSize` 并把角色加入忽略；
- 客户端改 `MaxWalkSpeed` 无效（服务器权威）→ 要走属性/GE；
- 蹲行时 Capsule 半高没改 → 站不起来或卡在天花板；
- `bOrientRotationToMovement` 与 `bUseControllerRotationYaw` 同时为 true → 角色抽搐。

✅ **自测**：急停急转不飘；贴墙时相机自动拉近不穿墙；蹲下能钻进低矮通道。

💼 **简历点**："调优第三人称移动手感（加速度/刹车/转向/相机延迟）并与服务器权威移动保持一致"。

---

## 阶段 4：地图灰盒（S15）

### S15 ✅ 军工厂灰盒 + 导航 + 出生点

🎯 **目标**：9 个分区连通、能跑能绕、Navigation Mesh 无断裂、5 个出生点、板/窗/机/椅占位摆放（先不放逻辑）。

📚 **要学会的概念**：
- 搜：`Level Instance`、`Modeling Tools 灰盒`、`NavMeshBoundsVolume`、`RecastNavMesh`、`P 键显示导航`、`Region Tag`（Actor Tag）
- 关键认知：**灰盒阶段不要碰美术**，用方块验证"空间节奏"（掩体密度、通路宽度、视线长度）。

📁 **要建的文件**：
```
Content/IDF/Maps/L_MilitaryFactory
Content/IDF/Maps/LI_Z1_CentralFactory ... LI_Z9_Midfield   （Level Instance）
Content/IDF/Art/Greybox/...                                  （灰盒材质与网格）
```

✍️ **你要写的逻辑**：
1. 按设计文档第 1 轮 6.2 节的分区表搭建（先摆大块，再抠细节）；
2. 每个分区的 `Actor Tag` 加 `Region.Z1.CentralFactory` 等（后面 AI 与统计要用）；
3. 放 `NavMeshBoundsVolume` 覆盖可玩区，按 `P` 检查导航是否连通；
4. 摆交互点占位（**先只摆位置，不放逻辑**）：7 台机位、2 个大门、12~16 张椅子、≥14 块板、≥8 个窗、3 个地窖候选点；
5. 用 `DA_MapLayout_MilitaryFactory`（数据资产）记录机位/椅位清单，方便后面批量生成与校验。

⚠️ **常见坑**：
- 灰盒阶段就把 Lumen 全开 → 编辑卡顿，先关掉动态 GI 提效率；
- 分区之间通路太窄 → 监管者过不去，追击玩法崩掉（通路净宽建议 ≥ 300cm）；
- 导航网格没覆盖到二层/楼梯 → AI 上不去；
- 板子倒地区域没预留 → 后面 `NavModifierVolume` 要重排。

✅ **自测**：跑图一圈无卡点；`P` 键看导航全绿；用 `testing_pie.md` 的 `IDF.Debug.Teleport` 快速巡场。

💼 **简历点**："按玩法节奏设计非对称对抗地图分区（每个区域具备破译位/转点位/视线阻断三要素），并完成导航与出生点公平性验证"。

---

## 阶段 5：交互点玩法（S16–S18）

> 到这里，**框架已经不用改了**。三个步骤都是在"填表格"：实现接口 + 写状态机 + 配数据。

### S16 ✅ 密码机完整版（含校准 QTE）

🎯 **目标**：7 台机、需破 5 台、校准 QTE 成功/失败、爆机退进度并暴露位置、多人协作加速。

📚 **要学会的概念**：
- 搜：`QTE 实现`、`客户端判定 vs 服务器判定`、`SetByCaller`（GE 传递数值）、`GameplayCue`（表现与逻辑分离）
- 关键认知：**QTE 的成败必须在服务器判定**，客户端只负责"显示指针 + 上报点击时刻"。

📁 **要建的文件**：
```
Plugins/GameFeatures/IDMMatch/Source/IDMMatch/Public/Machine/IDMMachineActor.h/.cpp
Plugins/GameFeatures/IDMMatch/Source/IDMMatch/Public/Machine/IDMDecodeComponent.h/.cpp
Content/IDF/StateTree/ST_Machine.uasset
Content/IDF/Abilities/GA_Survivor_Decode.uasset
```

✍️ **你要写的逻辑**：
1. 每 6~10s 随机触发一次校准（服务器决定时机与判定区位置，复制给客户端）；
2. 客户端显示旋转指针，玩家点击时上报**点击时刻**（或直接上报"我点了"，由服务器按当前时刻判定）；
3. 成功：+1% 进度加速 / 失败：退 8% 进度 + 给监管者暴露位置 5s；
4. 多人协作：`DecodeRate = Base * (1 + 0.5*(N-1))`（边际递减，数值见 RuleSet）；
5. 完成：计数 +1 → 广播事件 → 检查是否触发大门通电。

⚠️ **常见坑**：
- 客户端上报"点击成功"（而不是"我点了"）→ 直接可作弊，面试时会扣分；
- 校准圈位置/时机两端不一致 → 玩家看到的判定区和服务器判定不一致（体验灾难）；
- 退进度时对所有人都退（应该只退该机进度）；
- 忘了 `BlockAbilitiesWithTag`，导致能一边破译一边治疗。

✅ **自测**：故意点错校准，验证退进度 + 监管者 HUD 提示；5 端看到同一个校准圈。

💼 **简历点**："实现服务器权威的破译校准 QTE（时机与判定区服务器下发，客户端仅上报输入时刻），杜绝客户端伪造"。

---

### S17 ✅ 大门电闸 + 地窖 + 宝箱

🎯 **目标**：通电 → 开门读条（松手回落）→ 逃脱判定；地窖条件开启；宝箱服务器 Roll 道具。

📚 **要学会的概念**：
- 搜：`TriggerVolume`、`Server Roll 随机数`、`FRandomStream`（可复现随机）、`Client RPC 私有下发`
- 关键认知：**任何随机结果都必须在服务器生成**，且用 `FRandomStream` 便于复盘（"这局为什么开出这个道具"）。

📁 **要建的文件**：`IDMGateActor`、`IDMBasementActor`、`IDMChestActor`（`IDMMatch` 插件下）

✍️ **你要写的逻辑**：
1. 大门：`bPowered`（服务器在破译达标时置位）→ 读条进度 → 松手按 5%/s 回落 → 100% 后 `bOpened` + 门扇动画 + 逃脱触发体；
2. 地窖：服务器判定"剩 1 人 + 已破 ≥2 台"→ 从 3 个候选中激活 1 个 → 复制 `bActive` → 跳入即逃脱；
3. 宝箱：长按 4s → 服务器 `FRandomStream` Roll → `Client RPC` 只发给开箱者 + 广播表现。

⚠️ **常见坑**：
- 逃脱后直接 `Destroy()` 角色 → 结算时拿不到数据；应该隐藏 + 禁用碰撞 + 在 PlayerState 记录状态；
- 大门进度回落的速率符号写反 → 玩家松手后进度反而涨；
- 宝箱结果广播给所有人 → 泄露信息（原作只有开箱者知道开出了什么）。

✅ **自测**：破译 5 台后大门亮灯；开门读条松手会回落；只剩 1 人时地窖出现音效提示。

💼 **简历点**："实现大门通电-读条-回落-逃脱的完整状态机与地窖条件开启逻辑；宝箱道具用服务器随机流生成，保证可复现与不可预测"。

---

### S18 ✅ 狂欢之椅 + 救援（含简化挣扎，D4）

🎯 **目标**：挂椅三阶段、救援读条、二次上椅加速、简化挣扎延长计时、空椅 CD。

📚 **要学会的概念**：
- 搜：`AttachToComponent`（挂载）、`State 复制与超时`、`GetServerWorldTimeSeconds`（长时计时）、`Montage`（动画蒙太奇）
- 关键认知：**长时计时务必用服务器时间戳差值**，不要用 `Tick` 累加 `DeltaTime`（第 13 步提过，这里是它的真实场景）。

📁 **要建的文件**：`IDMChairActor`、`IDMChairTimerComponent`（`IDMMatch` 插件下）

✍️ **你要写的逻辑**：
1. 椅子状态机：`Idle → Occupied(Phase1) → Phase2 → Eliminated`，`Phase1 → Rescued → Idle(CD)`；
2. 计时用 `PhaseEndServerTime = GetServerWorldTimeSeconds() + Duration`，客户端用 `ServerTime` 换算剩余；
3. 挂椅：监管者扛人靠近 → 能力触发 → `AttachToComponent` 到座位 + 加 `State.Survivor.OnChair.*` 标签（禁移动/能力）；
4. 救援：队友 Hold 1s → 解除挂载 → 生命状态设为"受伤" → 给 2s 加速；
5. 简化挣扎（D4）：Phase2 期间 QTE 成功 → `PhaseEndServerTime += 2s`；
6. 空椅 CD 3s。

⚠️ **常见坑**：
- `AttachToComponent` 后碰撞还在 → 角色互相推挤，要 `SetActorEnableCollision(false)`；
- 客户端用本地时间算剩余 → 与服务器差几秒（必须走 `ServerTime`）；
- 救援完成瞬间又被挂回去（CD 没写）；
- 二次上椅应从 Phase2 起算，别忘了这个"加速惩罚"。

✅ **自测**：挂椅倒计时 5 端一致；救援后角色变为受伤态可被治疗；挣扎成功计时条变长。

💼 **简历点**："用服务器时间戳实现狂欢之椅的三阶段淘汰计时（替代 DeltaTime 累加，消除长时间漂移），并实现救援/简化挣扎/空椅冷却的完整交互闭环"。

---

## 阶段 6：角色实现（S19–S23）

### S19 ✅ 求生者公共：治疗 / 救援 / 道具栏

🎯 **目标**：治疗（自疗/被疗）、救援、道具拾取与使用三套通用能力可用，作为 4 个求生者的共享底座。

📚 **要学会的概念**：
- 搜：`GameplayAbility 生命周期`（`CanActivateAbility`/`ActivateAbility`/`CommitAbility`/`EndAbility`）、`AbilityTask`（`WaitDelay`/`WaitTargetData`）、`GameplayEffect 的 Duration/Infinite/Instant`、`GrantedTags`
- 关键认知：
  - `CommitAbility()` 负责扣成本与进 CD，**放在逻辑开始前**；
  - 持续型能力要用 `AbilityTask` + 委托，不要在能力里写 `Tick`；
  - 道具 = "临时授予一个能力"，用完移除（比 if-else 判断道具类型优雅得多）。

📁 **要建的文件**：`Plugins/GameFeatures/IDFSurvivor/`（新建插件）+ `GA_Survivor_Heal`、`GA_Survivor_Rescue`、`GA_Item_Use*`

✍️ **你要写的逻辑**：
1. `GA_Survivor_Heal`：Hold 型，`WaitDelay` 累计读条 → 完成时应用 `GE_Heal`；被打断时 `EndAbility(bWasCancelled=true)`；
2. 治疗速率走属性 `HealSpeed`（医生 +30% 就是改这个乘区）；
3. `GA_Survivor_Rescue`：目标为椅子上的队友，读条 1s；
4. `UIDFItemComponent`：`CurrentItemId` + `Uses`（复制），`GA_Item_Use` 按道具数据授予对应能力。

⚠️ **常见坑**：
- 忘记 `CommitAbility` → 技能没有 CD 也没有成本（"无限治疗"）；
- 能力被打断时没有清理（Timer/Delegate 泄漏）→ `EndAbility` 里统一清理；
- 道具用完没移除能力 → 还能继续用；
- 治疗进度只有自己看到（忘了同步给双方）。

✅ **自测**：自疗读条能被打断；被队友治疗时双方都看到进度；道具用完图标消失。

💼 **简历点**："把道具抽象为'临时授予能力'，避免道具逻辑的 if-else 爆炸；用 AbilityTask 实现可持续、可打断的读条类技能"。

---

### S20 ✅ 医生 / 幸运儿 / 律师 / 空军

🎯 **目标**：4 个求生者各有独立能力，且都以 **GameFeature 插件** 形式存在，禁用任一插件其余功能不受影响（AC-6）。

📚 **要学会的概念**：
- 搜：`GameFeature Plugin`、`UGameFeaturesSubsystem::LoadAndActivateGameFeaturePlugin`、`GameFeatureAction`（`AddComponents`/`AddAbilities`/`AddInputBinding`）、`UGameFrameworkComponentManager` 与 `IGameFrameworkInitStateInterface`
- 关键认知：GameFeature 的价值是**运行时按需加载 + 与主工程解耦**，这也是简历亮点。

📁 **要建的文件**：`IDFChar_Doctor`、`IDFChar_LuckyGuy`、`IDFChar_Lawyer`、`IDFChar_Airman`（每个含 `.uplugin` + `GameFeatureAction` + 角色类 + AbilitySet 资产）

✍️ **你要写的逻辑**（每个角色一条链路：能力 → 数据资产 → GameFeature 注入）：
| 角色 | 核心逻辑 | 重点难点 |
|---|---|---|
| 医生 | `GA_Doctor_SelfHeal` 8s 读条；`HealSpeed` ×1.3 | 与通用治疗能力的关系（继承还是独立） |
| 幸运儿 | `GA_LuckyGuy_Wish`：服务器 Roll 3 个候选 → `Client RPC` 下发 → 玩家选 → 上行确认 | **随机必须服务器做**；UI 是异步交互（等玩家输入） |
| 律师 | `GA_Lawyer_MarkMachine`：给目标机施加破译加速 GE；HUD 小地图 | **信息按客户端过滤**：只有律师收到监管者位置 |
| 空军 | `GA_Airman_FlareGun`：服务器生成投射物 → 命中 → `GE_Hunter_Stunned` | 投射物权威 + 客户端预测表现 |

⚠️ **常见坑**：
- GameFeatureAction 的注入时机（角色已生成后再加载插件会错过初始化）→ 用 `GameFeatureAction_AddComponents` 或 `InitState` 钩子；
- 幸运儿的"等待玩家选择"要用可恢复的能力（`WaitForEvent`/自定义 Task），别用 `Delay` 硬等；
- 律师的小地图数据如果广播给所有人 → 等于全队透视（破坏平衡）；
- 空军的眩晕是**双向**的：被眩晕者禁用输入，攻击者/旁观者看到特效。

✅ **自测**：逐角色验证能力；**禁用 `IDFChar_Doctor` 插件后**重新跑，其他角色与对局流程正常（这是 AC-6 的核心）。

💼 **简历点**："每个角色独立为 GameFeature 插件，通过 GameFeatureAction + 扩展点注入能力与输入，实现角色级模块化与按需加载"。

---

### S21 ✅ 监管者公共：攻击 / 擦刀 / 扛人 / 挂椅

🎯 **目标**：普通攻击命中判定、擦刀后摇、扛人、绑定到椅子，形成监管者的完整击杀链。

📚 **要学会的概念**：
- 搜：`球形/扇形攻击检测`（`SweepMultiByChannel` + 角度过滤）、`AnimNotifyState`（攻击窗口）、`Montage` 分段（前摇/判定/后摇）、`Root Motion`
- 关键认知：**攻击判定必须服务器做**，且判定窗口由动画通知驱动（不要用 `Delay`）。

📁 **要建的文件**：`Plugins/GameFeatures/IDFHunter/`（新建插件）+ `GA_Hunter_Attack`、`GA_Hunter_WipeBlade`、`GA_Hunter_Carry`、`GA_Hunter_PutOnChair`

✍️ **你要写的逻辑**：
1. 攻击：Montage 播放 → `AnimNotifyState` 开启判定窗口 → 服务器在窗口内做扇形 Sweep → 命中应用 `GE_Damage_NormalAttack`（`SetByCaller(Damage)`）；
2. 命中后进擦刀（`State.Hunter.WipingBlade`，1.5s），期间禁用其他能力；
3. 扛人：靠近倒地者 → 附带 `AttachToComponent` 到肩膀 + 移速降低；
4. 挂椅：扛人靠近椅子 → 触发 `AIDMChairActor` 的挂载流程。

⚠️ **常见坑**：
- 攻击判定用 `Delay` 而不是动画通知 → 动画改了判定就错位；
- 一个判定窗口内多次命中同一个目标 → 用 `HitActors` 数组去重；
- 命中判定在客户端做 → 直接作弊（"隔空打人"）；
- 擦刀期间还能翻窗/攻击 → 缺少 `ActivationBlockedTags`。

✅ **自测**：贴近攻击命中；背对攻击不命中；命中后 1.5s 内不能再次攻击；扛人时不能攻击。

💼 **简历点**："攻击判定由动画通知驱动 + 服务器扇形检测 + 单次窗口去重，保证判定与动画严格对齐且不可客户端伪造"。

---

### S22 ✅ 小丑：火箭冲刺（CharacterMovement 自定义模式）

🎯 **目标**：燃料消耗/回复、冲刺位移、撞墙惩罚、冲刺撞击伤害，且 150ms 延迟下不回弹（R1 风险点）。

📚 **要学会的概念**（**本项目最难的一步，建议留足时间**）：
- 搜：`FSavedMove_Character`、`UCharacterMovementComponent 自定义移动模式`、`FSavedMove_Character::CanCombineWith`、`ClientAdjustPosition`、`Network Prediction`、`MotionTrajectory`
- 关键认知：
  - 客户端预测位移的**唯一正规做法**是扩展 `FSavedMove_Character`，让移动组件"重放"你的自定义位移；
  - 用 `SetActorLocation` 做冲刺 → 客户端预测完全失效，会出现"冲过去又被拉回来"；
  - `CanCombineWith` 写错 → 网络包合并后位置错乱。

📁 **要建的文件**：`IDFChar_Clown/`（插件）+ `UClownMovementComponent`（`FSavedMove_Clown` + `FNetworkPredictionData_Client_Clown`）+ `GA_Clown_Dash`

✍️ **你要写的逻辑**：
1. `FSavedMove_Clown`：保存冲刺标志/方向/剩余时间，实现 `SetMoveFor`/`PrepMoveFor`/`CanCombineWith`/`GetCompressedFlags`；
2. `UClownMovementComponent`：自定义 `PhysCustom` 分支处理冲刺位移 + 碰撞 Sweep（撞墙 → 结束冲刺 + 眩晕）；
3. 燃料：`RocketFuel` 属性 + `GE_Clown_FuelCost`（Instant，`SetByCaller`）+ `GE_Clown_FuelRegen`（Infinite/Periodic）；
4. 冲刺撞击伤害：只在服务器做（避免客户端拉角度）。

⚠️ **常见坑**（这一节的坑最"贵"，逐条记）：
- 忘记实现 `CanCombineWith` → 快速连点冲刺时位移错乱；
- 冲刺中 `PhysCustom` 没设置 bJustTeleported 之类标志 → 位置校正异常；
- 客户端预测的燃料与服务器不一致 → UI 闪烁（燃料放 ASC 属性，让 GAS 帮你预测）；
- 撞墙判定用 `LineTrace` 单点 → 高速冲刺穿墙（要用 Sweep）。

✅ **自测**：`net PktLag=150 net PktLoss=3` 下冲刺全程无回弹、撞墙正确触发眩晕；客户端燃料显示与服务端一致。

💼 **简历点**："用 UCharacterMovementComponent 的 FSavedMove 扩展实现可预测的高速冲刺（而非 SetActorLocation），在 150ms 延迟 + 3% 丢包下无位置回弹"——**简历里最有说服力的一条**。

---

### S23 ✅ 红蝶：相态切换 + 冲刺 + 蝶印瞬移

🎯 **目标**：美人相/般若相切换（改移速与动作速度）、刹那生灭（短冲刺）、燕返（标记 + 瞬移到目标身后），瞬移落点服务器裁定。

📚 **要学会的概念**：
- 搜：`瞬移的网络同步`、`IsNetRelevantFor`（按客户端过滤可见性）、`Camera Blend`（瞬移时的镜头过渡）、`Niagara 残影`
- 关键认知：**瞬移 = 服务器决定落点 + 客户端播表现 + 相机跟着混合**；客户端无权决定落点。红蝶的蝶印"只有红蝶能看到"→ 这是 `IsNetRelevantFor` 或 `Client RPC` 的用武之地。

📁 **要建的文件**：`IDFChar_Geisha/`（插件）+ `GA_Geisha_Phase`、`GA_Geisha_Dash`、`GA_Geisha_Mark`、`GA_Geisha_Blink`

✍️ **你要写的逻辑**：
1. 相态切换：两个 GE 分别改 `MoveSpeedMultiplier` 与 `ActionSpeedMultiplier`；
2. 刹那生灭：短距离位移（**复用 S22 的 FSavedMove 机制**，别重写一遍）；
3. 燕返：服务器校验视线与距离 → 打 `State.Hunter.Mark.Target` 标签 → 再次激活时服务器用 `SetActorLocation` + `bJustTeleported` 瞬移（**仅服务器，客户端跟随后校正**）+ Niagara 残影；
4. 相机：瞬移时用 `GameplayCameras` 的 Camera Rig 做一次快速混合，避免玩家眩晕。

⚠️ **常见坑**：
- 瞬移用客户端 `SetActorLocation` → 服务器端位置不同步（"我明明在楼上"）；
- 射线检查视线时忘了忽略自己与目标；
- 标记对所有客户端可见 → 求生者能看到自己被标记（红蝶的机制就废了）；
- 瞬移后相机瞬间跳 → 必须做混合过渡。

✅ **自测**：瞬移后 5 端位置一致；只有红蝶窗口能看到蝶印标记；瞬移镜头不晕。

💼 **简历点**："实现瞬移类技能的服务器裁定 + 客户端表现分离，并用网络相关性过滤实现'仅自己可见'的技能标记"。

---

## 阶段 7：流程 UI、AI 与打磨（S24）

### S24 ✅ 最小流程 + 最小 AI + 打包前打磨

🎯 **目标**：选人 → 对局 → 结算能走完；监管者 AI 能巡逻/追击/攻击；验收文档全部通过。

📚 **要学会的概念**：
- 搜：`CommonUI ActivatableWidget`、`PrimaryGameLayout`、`MVVM ViewModel`、`AIController + BehaviorTree + Blackboard + EQS`、`AIPerceptionComponent`、`Gauntlet 自动化测试`
- 关键认知：UI 只读 ViewModel、绝不直接改游戏状态；AI 的"高层决策用 StateTree、具体行为用 BT"。

📁 **要建的文件**：`IDFUI/`（插件，含 HUD/选人/结算）、`AIDFHunterAIController` + `BT_Hunter` + `BB_Hunter` + `EQS_*`

✍️ **你要写的逻辑**：
1. CommonUI 三层栈（Game/Menu/Modal）+ 输入模式随栈自动切换；
2. HUD：剩余密码机、队友状态、技能 CD、交互提示（复用 S11 的 ViewModel）；
3. 选人：6 个按钮 + `Server_RequestCharacterSelect`（**此时才真正做 GameFeature 动态加载**）；
4. 结算：逃脱/淘汰统计；
5. AI：感知（视觉+听觉）→ 黑板 → BT 状态（巡逻/追击/攻击/扛人/守椅）+ EQS 找板窗；
6. 用 Gauntlet 跑 1 真人 + 4 AI 的完整对局。

⚠️ **常见坑**：
- UI 里写 `GetAuthGameMode()` → 客户端崩溃（GameMode 只在服务器存在，**这是新手最经典的崩溃**）；
- CommonUI 的输入模式没切 → 打开菜单后角色还在动；
- AI 的 `MoveTo` 目标每帧重算 → 抖动（要节流）；
- 忘了导航网格 → AI 原地转圈。

✅ **自测**：`testing_pie.md` 全部用例 + `design_analysis.md` 第 1 轮 AC-1~AC-7。

💼 **简历点**："用 CommonUI 三层栈 + MVVM 实现 UI 与游戏状态解耦（客户端不访问仅服务器存在的 GameMode）；基于 BT + EQS 实现监管者 AI 的巡逻/追击/守椅决策"。

---

## 附录 A：全程必须建立的 6 个习惯

| # | 习惯 | 为什么 |
|---|---|---|
| 1 | **先写数据资产，再写逻辑** | 数值外置是设计文档 ADR-6；改平衡不用重编译 |
| 2 | **每个能力都写清 5 个标签**（`AbilityTags` / `ActivationOwnedTags` / `ActivationBlockedTags` / `CancelAbilitiesWithTag` / `BlockAbilitiesWithTag`） | 互斥与打断全靠标签，漏一个就是"能一边倒地一边治疗" |
| 3 | **每完成一个网络功能，就用 `net PktLag=150 net PktLoss=3` 复测** | 局域网顺畅不代表联机正确，问题都是延迟暴露的 |
| 4 | **每次提交前跑一遍 5 窗口 + 关掉一个 GameFeature 插件** | 验证解耦（AC-6），也验证你没写出隐式依赖 |
| 5 | **每个里程碑用 `stat net` / `GameplayInsights` / `Unreal Insights` 各截一张图** | 简历里的量化数据就是这么攒出来的 |
| 6 | **每天结束写 3 行 devlog**（做了什么 / 卡在哪 / 明天做什么） | 既服务简历，也服务"下次接着干"的记忆恢复 |

---

## 附录 B：报错时怎么找我（提高效率的提问模板）

把下面四项填好发我，我能一次给到定位：

```
1. 我在做哪一步：S12 服务器校验
2. 我改了什么：在 IDFInteractionComponent.cpp 里加了 Server_BeginInteraction_Implementation
3. 报错/现象：编译报错 / 运行时崩溃 / 行为不对（贴完整错误或日志片段）
4. 我已经试过：重新编译、清理 Intermediate、换 Standalone 测试
```

如果是**运行时行为不对**（不崩但不对），再补两条：
- 是 Standalone 还是 Listen Server + Client？（很多问题只在客户端出现）
- `stat net` / `showdebug abilitysystem` 里看到什么？

---

## 附录 C：常用参考（自己去查，比我讲更靠谱）

| 主题 | 搜索关键词 |
|---|---|
| GAS 入门 | `Unreal Engine Gameplay Ability System documentation`、`GASDocumentation tranek`（社区最全的 GAS 中文/英文教程） |
| GAS 初始化时序 | `InitAbilityActorInfo PossessedBy OnRep_PlayerState` |
| 网络同步 | `Unreal Engine Networking Overview`、`Actor Replication`、`RPC`、`Client-Side Prediction` |
| 移动预测 | `FSavedMove_Character`、`Custom Character Movement Component` |
| GameFeature | `Game Features and Modular Gameplay`（Epic 官方 Lyra 文档） |
| StateTree | `State Tree Unreal Engine`、`GameplayStateTreeModule` |
| CommonUI | `CommonUI Quick Start`、`PrimaryGameLayout` |
| AI | `Behavior Tree Quick Start`、`EQS`、`AI Perception` |
| 性能/网络分析 | `Unreal Insights`、`stat net`、`Gameplay Insights` |

---

## 附录 D：进度勾选表

| 步骤 | 内容 | 状态 |
|---|---|---|
| S01 | 补齐插件 + 首次重编译 | ☐ |
| S02 | Git + LFS | ☐ |
| S03 | 项目配置 | ☐ |
| S04 | 日志与调试基础设施 | ☐ |
| S05 | 最小 GAS 骨架（ASC + AttributeSet） | ☐ |
| S06 | AbilitySet + InputConfig + 原生标签 | ☐ |
| S07 | 角色基类 ASC 双端初始化 | ☐ |
| S08 | PlayerState/GameState/GameMode/PlayerController | ☐ |
| S09 | 里程碑：5 窗口 + 移动同步 | ☐ |
| S10 | 交互契约（接口 + 结构体） | ☐ |
| S11 | 交互组件：客户端扫描 + UI 提示 | ☐ |
| S12 | 交互组件：服务器校验 + 预测 | ☐ |
| S13 | 假密码机 + 进度同步 | ☐ |
| S14 | 第三人称移动与相机 | ☐ |
| S15 | 军工厂灰盒 + 导航 | ☐ |
| S16 | 密码机完整版 + 校准 QTE | ☐ |
| S17 | 大门 + 地窖 + 宝箱 | ☐ |
| S18 | 狂欢之椅 + 救援 | ☐ |
| S19 | 求生者公共（治疗/救援/道具） | ☐ |
| S20 | 医生/幸运儿/律师/空军 | ☐ |
| S21 | 监管者公共（攻击/擦刀/扛人/挂椅） | ☐ |
| S22 | 小丑（可预测冲刺） | ☐ |
| S23 | 红蝶（相态/冲刺/瞬移） | ☐ |
| S24 | 流程 UI + 最小 AI + 打磨 | ☐ |

> **下一步**：从 S01 开始。如果你希望，我可以先带你过一遍 S01 的编译流程（包括命令行编译 vs 编辑器编译的区别、怎么看编译日志里的关键行）。
