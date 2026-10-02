# S03 实施步骤：项目配置

> 对应 `implementation_roadmap.md` 的 S03。**目标**：让编辑器启动就用对的地图与 GameMode，网络参数就位，并且你**理解 ini 配置的加载机制**（这是面试常问、也是日常最容易踩坑的地方）。
> 本节预计 1~2 小时，其中一半时间在"验证配置真的生效"。

---

## 0. 先搞懂：UE 的 ini 分层与优先级（这一节比改配置本身更重要）

一个 UE 项目的运行配置，是**多层 ini 按顺序合并**出来的。从低到高：

| 层级 | 路径 | 作用 | 你会改吗 |
|---|---|---|---|
| 1. 引擎基础 | `Engine/Config/Base*.ini` | 引擎出厂默认值 | ❌ 永远不要改（改引擎会污染所有项目） |
| 2. **项目默认** | `项目/Config/Default*.ini` | **项目的真相，要提交 Git** | ✅ 本节改这里 |
| 3. 平台覆盖 | `项目/Config/Windows/WindowsEngine.ini` | 按平台差异覆盖 | ⬜ 本作暂不需要 |
| 4. **用户级** | `项目/Saved/Config/WindowsEditor/*.ini` | 编辑器**自动写入**的个人设置（窗口布局、PIE 设置、面板开合） | ⚠️ 不要手工当配置改，见下方陷阱 |
| 5. 命令行 | `-ini:...` / `-ExecCmds` | 临时覆盖 | ⬜ 调试时偶尔用 |

**合并规则**（三种语法要分清楚）：

```ini
Key=Value          ; 覆盖（默认行为）
+Key=Value         ; 追加到数组
-Key=Value         ; 从数组移除
!Key=ClearArray    ; 清空数组后再追加
```

**头号陷阱（必读）**：你机器上**已经存在**这些用户级文件：

```
Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini
Saved/Config/WindowsEditor/Editor.ini
Saved/Config/WindowsEditor/GameUserSettings.ini
```

它们**优先级高于** `Config/Default*.ini`。所以如果你在编辑器里改了一个设置、又发现 `DefaultEngine.ini` 里没有对应变化，那个值很可能被写进了 `Saved/` —— 而 `Saved/` 是被 `.gitignore` 忽略的，**不会同步给其他人，也不进简历项目**。

**判断方法**：改完配置后如果在 `Default*.ini` 里找不到自己刚改的值，就去 `Saved/Config/` 里搜。

---

## 1. 现状体检（我已经帮你查过，你只需理解）

| 文件 | 现状 | 问题 |
|---|---|---|
| `Config/DefaultEngine.ini` | `GameDefaultMap=/Engine/Maps/Templates/OpenWorld` | ❌ 指向**引擎自带的模板地图**（不是我们的地图），这是从模板继承来的残留 |
| 同上 | **没有** `EditorStartupMap` | ⚠️ 缺省时用引擎默认（也是 OpenWorld 模板），启动编辑器会自动打开一个引擎地图 |
| 同上 | `GlobalDefaultGameMode=...Engine.GameModeBase`（继承引擎默认） | ❌ 项目**没有自己的 GameMode**，所以 PIE 时用的是空 GameMode：不会正确生成玩家、没有 GameState 逻辑 |
| 同上 | `ServerDefaultMap` 缺失 | ⚠️ 缺省值为 `/Engine/Maps/Entry`，本轮用不到，但以后开专用服会用到 |
| 同上 | 没有任何 `net.*` 设置 | ⚠️ 需要用 `[SystemSettings]` 段配置 |
| `Config/DefaultGame.ini` | 只有 CommonUI 设置 + `ProjectID` | ⚠️ 缺 `ProjectName`（影响窗口标题与打包） |
| `Config/DefaultInput.ini` | 模板默认，已启用 EnhancedInput | ✅ 无需改（大量 VR AxisConfig 是无害冗余，S14 再清理） |
| `Config/DefaultEditor.ini` | 空文件 | ✅ 保持空即可 |

---

## 2. 本节要做的三件事

### 2.1 建一个"入口 GameMode"的 C++ 类

**为什么必须先做这一步**：如果你现在就把
```ini
GlobalDefaultGameMode=/Script/Identity5.Identity5GameMode
```
写进配置，PIE 时会因为**找不到这个类**而报错（`Failed to load class`），甚至直接回退到默认 GameMode，你会以为"配置没生效"。

**更重要的架构原因**（这点面试会问）：`GameMode` 是**只在服务器存在**、且由**配置文件用硬引用**指定的类。而设计文档里真正的对局 GameMode（`AIDMGameMode`）属于 **IDMMatch GameFeature 插件**——插件是**运行时才加载**的，配置文件在引擎启动早期就要解析 GameMode 类名，**此时插件还没加载**。所以配置里必须指向一个"永远可用的入口类"。

于是采用**两层 GameMode** 设计：

```
Config/DefaultEngine.ini                ← 指向主模块的入口类（永远存在）
  GlobalDefaultGameMode=/Script/Identity5.Identity5GameMode

L_MilitaryFactory 的 World Settings     ← 地图级覆盖为插件里的对局 GameMode（编辑器里选，不是写 ini）
  GameMode Override = /Script/IDMMatch.IDMGameMode
```

**为什么地图级覆盖能引用插件类**：`WorldSettings` 里存的是**资产内的引用**，地图加载时插件已经加载完毕，不存在时序问题。而 ini 是引擎启动早期的文本解析，所以只能放主模块的类。

📁 **要建的文件**：

```
Source/Identity5/Identity5GameMode.h
Source/Identity5/Identity5GameMode.cpp
```

✍️ **你要写的代码**（这是本项目第一个 C++ 类，我给出完整骨架，因为它是"样板代码"而非逻辑，你需要理解的是**每一部分为什么存在**）：

```cpp
// Identity5GameMode.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Identity5GameMode.generated.h"

/**
 * 项目入口 GameMode（主模块，永远可用的"占位/兜底"实现）。
 *
 * 为什么需要它：
 *  - GameMode 由 Config/DefaultEngine.ini 用"硬类路径"指定，而 ini 在引擎启动早期被解析，
 *    此时 GameFeature 插件尚未加载，因此不能直接指向 IDMMatch 插件里的对局 GameMode。
 *  - 所以入口指向主模块的这个类；真正的对局 GameMode 通过地图的 World Settings 覆盖指定。
 */
UCLASS(Config = Game)
class IDENTITY5_API AIdentity5GameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AIdentity5GameMode();
};
```

```cpp
// Identity5GameMode.cpp
#include "Identity5GameMode.h"

AIdentity5GameMode::AIdentity5GameMode()
{
    // 本轮：只做基类默认行为（生成默认 Pawn、默认 PlayerController、默认 HUD）。
    // 后续 S08 会由 IDMMatch 插件里的 AIDMGameMode 继承或替代它。
}
```

⚠️ **注意三个必须点**（写错就编译失败）：

| 项 | 说明 |
|---|---|
| `#include "Identity5GameMode.generated.h"` | **必须是最后一个 include**，且文件名与头文件同名。这是 UHT 生成的反射代码 |
| `GENERATED_BODY()` | 放在 `UCLASS` 内的第一行 |
| `IDENTITY5_API` | 模块导出宏，等于 `模块名大写_API`。你的模块名是 `Identity5`，所以是 `IDENTITY5_API`。**漏了它会导致其他模块无法引用这个类** |

❓ **`UCLASS(Config = Game)` 里的 `Config` 是什么意思？**
它让这个类里标记了 `UPROPERTY(Config)` 的成员可以被写进 `DefaultGame.ini`。现在类里没有这种成员，但将来配"默认规则集"时会用到，所以先留着。

> ⚠️ **不要给这个类加 `Abstract`（我最初写错了，已修正）**：
> `Abstract` 表示"这个类不能被实例化，只能被继承"。而 PIE 启动时**必须真正生成一个 GameMode actor**（它既是配置里的入口 GameMode，也是没有被地图 `World Settings` 覆盖时的兜底），所以加了 `Abstract` 会直接导致 PIE 失败：
> ```
> LogLoad:  Game class is 'Identity5GameMode'
> LogSpawn: Warning: SpawnActor failed because class Identity5GameMode is abstract
> LogWorld: Error: Failed to spawn GameMode actor.
> ```
> 紧接着**编辑器会弹出一个和根因毫无关系的对话框**：
> ```
> 编辑器预览场景创建失败。
> ```
> **这个弹窗是误导性的**——它是"PIE 会话初始化失败"的通用提示。以后遇到它，**第一件事是去 `Saved/Logs/Identity5.log` 里搜 `Failed to spawn`**，真正的原因就在它上面一两行。

**编译**：关闭编辑器 → 在项目根目录用 S01 学到的 `Build.bat` 命令编译 → 打开编辑器。

---

### 2.2 改 `Config/DefaultEngine.ini`

**只改两处，其余保持不动**（你文件里 `[/Script/Engine.RendererSettings]` 那一大段是模板生成的渲染设置，Lumen/Nanite/VSM 都合理，**不要动**）。

**改动一**：把文件最上面的地图/GameMode 段替换掉。

找到这三行（在文件开头）：

```ini
[/Script/EngineSettings.GameMapsSettings]
GameDefaultMap=/Engine/Maps/Templates/OpenWorld
```

替换为：

```ini
[/Script/EngineSettings.GameMapsSettings]
; 编辑器启动时打开的关卡（进入编辑器 → 直接打开这张图）
EditorStartupMap=/Game/IDF/Maps/L_MilitaryFactory

; 独立运行/PIE 时加载的默认关卡
GameDefaultMap=/Game/IDF/Maps/L_MilitaryFactory

; 专用服务器（Dedicated Server）的默认关卡
; 本作当前只用 PIE 的 Listen Server，暂不需要；将来做无头服务器压测时启用
; ServerDefaultMap=/Game/IDF/Maps/L_MilitaryFactory

; 入口 GameMode：主模块的类（永远存在，不受插件加载时序影响）。
; 注意路径格式：/Script/<模块名>.<去掉 A 前缀的类名>，没有 .h，也没有 A。
GlobalDefaultGameMode=/Script/Identity5.Identity5GameMode

; 本作固定 4v1，不做多人自定义模式；这项保持引擎默认即可
; GameInstanceClass=/Script/Engine.GameInstance
```

**改动二**：在文件末尾追加网络配置段。

⚠️ **关键**：`/Script/Engine.RendererSettings`、`/Script/EngineSettings.GameMapsSettings` 这些是**各自的配置段**，`net.*` 这类**控制台变量**必须放在 `[SystemSettings]` 段里，否则不会生效。

在文件**末尾**追加：

```ini
; ══════════════════════════════════════════════════════════════
; 系统控制台变量（网络同步相关）
; 注意：net.* 是控制台变量，必须放在 [SystemSettings] 段
; ══════════════════════════════════════════════════════════════
[SystemSettings]
; 自适应网络更新频率：根据 Actor 实际是否发送数据来动态调整 NetUpdateFrequency，
; 对"平时不动、偶尔变化"的交互点（密码机、椅子）能显著省带宽。
; UE5.8 的 C++ 默认值是 0（关闭），所以我们显式打开。
; 已核实：Runtime/Engine/Private/NetDriver.cpp 中 CVarUseAdaptiveNetUpdateFrequency 默认 0。
net.UseAdaptiveNetUpdateFrequency=1

; ⚠️ 不要再写 net.MaxNetUpdateFrequency：在 UE5.8 中该 cvar 已不存在
;（全引擎源码搜索无结果）。全局上限已改为在 Actor 上控制：
;   使用 AActor::SetNetUpdateFrequency(float) / GetNetUpdateFrequency()
; 注意：UE5.5 起 NetUpdateFrequency 属性被标记为 deprecated，必须走 Setter/Getter。
```

**为什么 `net.UseAdaptiveNetUpdateFrequency` 值得单独解释**（面试点）：
它让引擎统计每个 Actor"实际多久发一次有效数据"，然后自动降低那些长期不变化的 Actor 的更新频率。对本作特别有价值，因为**密码机有 7 台、椅子有 12~16 张**，它们绝大多数时间状态不变——如果都给 30Hz 复制，纯属浪费。这也是设计文档 ADR-7"离散增量复制"在引擎层面的补充手段。

---

### 2.3 改 `Config/DefaultGame.ini`

先做个小实验：打开 `Engine/Config/BaseGame.ini` 搜 `ProjectName`，你会看到引擎默认是**空字符串**。项目的"名字"直接决定**运行时窗口标题**，打包后也用它做元数据。

在 `Config/DefaultGame.ini` 的 `[/Script/EngineSettings.GeneralProjectSettings]` 段里补齐字段（**保留已有的 `ProjectID`，不要删**——它是项目唯一标识，删了会被重新生成，可能影响资产引用）：

```ini
[/Script/EngineSettings.GeneralProjectSettings]
ProjectID=607BC94D4747E0C84B582C9846699F27
ProjectName=Identity5
ProjectVersion=0.1.0
CompanyName=
Description=UE5 非对称对抗玩法复刻项目（学习性质，非商用）
CopyrightNotice=
```

⚠️ **`ProjectName` 的影响面**：
- 运行时窗口标题（`Identity5 (64-bit) ...`）；
- 打包输出目录与可执行文件名；
- **不要用中文字符**，避免打包时文件系统编码问题。

**CommonUI 那一段不要动**（`[/Script/CommonUI.CommonUISettings]` 与 `[ConsoleVariables]` 里的 `CommonUI.*`）——它们是 CommonUI 插件需要的，S24 做 UI 时会用到。

---

## 3. 注意：不要用编辑器 UI 改这些设置

编辑器里有 **编辑 → 项目设置（Project Settings）**，里面确实有对应面板：

| 面板 | 对应 ini 段 |
|---|---|
| **Project → Maps & Modes** | `[/Script/EngineSettings.GameMapsSettings]` |
| **Project → Description** | `[/Script/EngineSettings.GeneralProjectSettings]` |
| **Project → Packaging** | `[/Script/UnrealEd.ProjectPackagingSettings]` |

**两种方式的区别**（这是很实用的工程经验）：

| | 手改 ini | 用编辑器 UI |
|---|---|---|
| 改了什么 | 只有你写的那几行 | 会把**整个段**重写一遍，可能带出你没注意的默认值、改变键的顺序 |
| Diff 可读性 | 干净、一眼看出改了什么 | 一大片变化，Code Review 时干扰 |
| 风险 | 拼写错误不会报错，只是"不生效" | 有下拉框约束，不易拼错 |

**本节建议**：按上面手改 ini。改完**用编辑器 UI 反向验证**（打开 Project Settings → Maps & Modes，看界面上显示的路径是否就是你写的）——这是最省事的"确认配置被正确解析"的方法，比翻日志快。

---

## 4. 自测（4 项，逐项做）

### 自测 1：类路径能被解析（**最关键**）

关闭编辑器 → 编译 → 打开编辑器。

打开 **Project Settings → Maps & Modes**：

| 字段 | 期望显示 |
|---|---|
| Editor Startup Map | `L_MilitaryFactory` |
| Game Default Map | `L_MilitaryFactory` |
| Default GameMode | `Identity5GameMode` |

**如果 Default GameMode 显示为 `GameModeBase` 或空白** → 说明类路径写错或类没编译进去。检查：
1. `.h` 里是不是 `IDENTITY5_API`（漏了就编译不过，但如果是别的模块访问才会报错）；
2. 路径拼写：`/Script/Identity5.Identity5GameMode`（模块名大小写敏感）；
3. 是否真的编译成功（看 `Binaries/Win64/` 下 dll 的时间戳是否更新）。

### 自测 2：日志确认

PIE 一次（此时地图可能还不存在，见下方"预期报错"），在 **Output Log** 里搜：

```
LogLoad:  (Engine Initialization)
LogGameMode: Display: Match State Changed from EnteringMap to WaitingToStart
```

你会看到 GameMode 的类名。或者直接在 PIE 里按 `~` 打开控制台输入：

```
GetAll /Script/Identity5.Identity5GameMode
```

### 自测 3：地图不存在时的预期行为

**现在还没有 `L_MilitaryFactory`**（S15 才建），所以 PIE 会报：

```
Warning: Failed to load map '/Game/IDF/Maps/L_MilitaryFactory'
```

**这是正常的、预期内的**，说明配置确实被读取了（它真的去尝试加载了）。此时你可以：
- 先建一张临时地图 `Content/IDF/Maps/L_Test.umap`（一个空关卡 + 一块地板 + 5 个 PlayerStart），把配置里的路径暂时改成它；
- 或者接受这个警告，等 S15 建好地图。

> **建议建临时地图**：S09 要用 5 个 PlayerStart 做联机验收，现在建好能提前用上，而且能立刻验证 GameMode 是否真的生效。记得配置里的路径与地图实际路径保持一致。

### 自测 4：`net.*` 变量生效

PIE 中按 `~` 输入：

```
net.UseAdaptiveNetUpdateFrequency
```

期望输出当前值 = `1`（带一句帮助文本）。如果显示 0，说明 `[SystemSettings]` 段的位置或拼写有问题。

---

## 5. 常见坑（本节特有）

| 现象 | 原因 | 处理 |
|---|---|---|
| 改了 `DefaultEngine.ini` 完全不生效 | 同一个键在 `Saved/Config/WindowsEditor/*.ini` 里有更高优先级的覆盖 | 去 `Saved/Config/` 搜该键名；或临时把 `Saved/Config/WindowsEditor/` 改名排除干扰（编辑器会重建） |
| 报 `Failed to load class '/Script/Identity5.Identity5GameMode'` | 类没编译 / 模块名写错 / 漏 `IDENTITY5_API` | 确认编译产物时间戳；确认路径格式 |
| 类路径写成 `/Script/Identity5.AIdentity5GameMode` | 记错了格式 | 配置里的类路径**不带 `A` 前缀**（那是 C++ 变量命名习惯，不是反射名） |
| 路径写成 `Identity5GameMode.h` 或带 `.h` | 记混了 | 配置里只有 `/Script/模块名.类名` |
| `[SystemSettings]` 里的变量不生效 | 写成了 `[SystemSettings]` 之外的段名，或写在了 `/Script/...` 段里 | 段名必须精确为 `[SystemSettings]`，且不能嵌在其他段内 |
| ini 里用 `//` 写注释 | ini 的注释符是 `;` | `//` 会被当成值的一部分，导致解析异常 |
| 编辑器里改了设置，`git diff` 一片红 | 编辑器重写了整个段 | 用 `git diff` 逐段看，把无关变化手动还原（或干脆只用手改 ini） |
| 保存 ini 后编辑器没反应 | 大部分 ini 需要**重启编辑器**才生效；少数（如 `[SystemSettings]` 的 cvar 默认值）需要重启 | 重启编辑器再验证 |
| `ProjectName` 改了窗口标题没变 | 标题里还带着配置名与位数 | UE 的标题格式是 `ProjectName [Configuration] (Bitness) Editor`，确认看的是项目名部分 |
| **PIE 弹窗"编辑器预览场景创建失败"** | **这个文案是误导性的**，它只是"PIE 会话初始化失败"的通用提示。真正的原因在日志里它上面一两行 | 打开 `Saved/Logs/Identity5.log` 搜 `Failed to spawn`。本项目实测到的根因是：`LogSpawn: Warning: SpawnActor failed because class Identity5GameMode is abstract` → GameMode 被声明成了 `Abstract`，PIE 无法实例化它。去掉 `UCLASS` 里的 `Abstract` 重新编译即可 |
| 日志报 `Asset manager settings do not include a rule for assets of type GameFeatureData` | 启用 Game Features 插件后，资产管理器需要一条 `GameFeatureData` 的扫描规则 | 引擎弹窗问"是否添加"时选**是**；或手动在 `DefaultGame.ini` 的 `[/Script/Engine.AssetManagerSettings]` 段加 `+PrimaryAssetTypesToScan=(PrimaryAssetType="GameFeatureData",AssetBaseClass=/Script/GameFeatures.GameFeatureData,bHasBlueprintClasses=False,bIsEditorOnly=False,Directories=((Path="/Game")),Rules=(Priority=-1))` |
| 日志报 `Using CommonUI without a CommonGameViewportClient derived game viewport client` | 启用了 CommonUI 插件，但还没建自定义 `GameViewportClient`（S24 才建） | 暂时忽略；嫌刷屏就在 `DefaultEngine.ini` 的 `[SystemSettings]` 加 `CommonUI.Debug.CheckGameViewportClientValid=0` |

---

## 6. 提交

```powershell
cd 'G:\Project\Unreal Projects\Identity5'

git add Config/DefaultEngine.ini Config/DefaultGame.ini Source/Identity5/Identity5GameMode.h Source/Identity5/Identity5GameMode.cpp
git commit -m "chore(config): 配置默认地图与入口 GameMode，启用自适应网络更新频率；新增入口 GameMode 类"
```

> 注意：这条命令**故意不包含** `Saved/` 下的任何东西（已被 ignore），也不包含 `.sln`（重建即可）。

---

## 7. 这一步的简历点与面试问答

💼 **可以这样写**：

> **项目配置与启动装配**：设计"配置文件 + 地图 World Settings"的两层 GameMode 装配方案——因 GameMode 由 ini 以硬类路径在引擎启动早期解析，而 GameFeature 插件此时尚未加载，故入口 GameMode 放在主模块，实际对局 GameMode 由地图 World Settings 覆盖指定；并启用 `net.UseAdaptiveNetUpdateFrequency` 配合离散增量复制降低交互点带宽。

**可能会被追问的三个问题，先想清楚答案**：

1. **"`+Key=` 和 `Key=` 有什么区别？"**
   → `Key=` 是覆盖标量值；`+Key=` 是往数组里追加一项；`-Key=` 从数组移除；`!Key=ClearArray` 先清空再追加。例如 `+NetDriverDefinitions` 就是追加。

2. **"为什么 GameMode 不直接放在插件里？"**
   → 因为 ini 在引擎启动早期解析，`GlobalDefaultGameMode` 是硬类引用，此时 GameFeature 插件未加载；把入口类放主模块，用地图级 `WorldSettings` 覆盖指向插件类，绕开时序问题。这也是为什么 Lyra 用"Experience"资产而不是直接写 GameMode 路径。

3. **"被忽略的 `Saved/Config/` 为什么危险？"**
   → 它优先级比 `Config/Default*.ini` 高，编辑器会自动往里写用户设置。如果团队里有人把共享设置改成了用户级设置，就会出现"我这里是好的、别人那里不对"，而且因为它被 git 忽略，问题无法通过提交修复。

---

## 8. 本节的"下一步"

S03 完成后，`implementation_roadmap.md` 的 S04 是**日志与调试基础设施**——建议不要跳过它，因为从 S05（GAS）开始你会频繁需要"按自己的日志类别定位问题"，先建好工具链能省掉大量排查时间。
