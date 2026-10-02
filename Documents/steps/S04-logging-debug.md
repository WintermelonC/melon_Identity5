# S04 实施步骤：日志与调试基础设施

> 对应 `implementation_roadmap.md` 的 S04。**目标**：在写任何 GAS 代码之前，先把"能看清程序在干什么"的工具搭好。
> 本节预计 2~3 小时。**不要跳过**——从 S05 开始，你会大量依赖自己的日志类别来定位"技能为什么没激活""属性为什么没复制"这类**没有任何报错**的问题。

---

## 0. 为什么必须先做这件事（两个真实场景）

**场景 A：为什么我的日志被淹没了**

UE 启动一次会输出几百行日志（Shaders、AssetRegistry、Slate……）。你写 `UE_LOG(LogTemp, Warning, ...)` 的时候，这句话会被埋在中间。更糟的是排查"技能为什么没生效"时，你需要按开关临时打开详细输出——如果日志都挤在 `LogTemp` 里，你只能靠 `grep` 猜。

**场景 B：为什么我的工程突然链接失败**

如果你在两个 `.cpp` 里各写一次 `DEFINE_LOG_CATEGORY(LogIDFNet)`，链接器会报 `duplicate symbol`。日志类别是**一个全局变量**，必须"头文件声明一次、cpp 里定义一次"。

**本节要建立的三样东西**：

| 工具 | 解决什么问题 | 什么时候你会用到 |
|---|---|---|
| **分层日志类别** | 按系统过滤输出、按需调详细级别 | S05 起全程（GAS 初始化、属性复制、交互校验） |
| **屏幕调试显示** | 不打断游戏就能看到运行时数值 | S09（看阵营/网络模式）、S13（看破译进度） |
| **控制台变量与命令** | 运行时开关行为，不用重编译 | S11（调试可视化）、S13（`IDF.Debug.Interact`） |

---

## 1. 必须先理解的四个概念

### 1.1 日志的五个级别（`ELogVerbosity`）

| 级别 | 含义 | 默认是否输出 | 用在什么场景 |
|---|---|---|---|
| `Fatal` | 致命错误，**会崩溃** | 是 | 几乎不用（例外：数据表缺失导致无法继续） |
| `Error` | 错误 | 是 | 预期外的失败（服务器校验发现非法状态） |
| `Warning` | 警告 | 是 | 可疑但不致命（请求被拒绝、资源缺失） |
| `Log` | 常规信息 | **是** | 关键流程节点（对局阶段切换、角色生成） |
| `Verbose` | 详细信息 | **否** | 高频/噪音信息（每 0.1s 的交互扫描、每次属性变化） |
| `VeryVerbose` | 更详细 | 否 | 逐帧级别的调试 |
| `Display` | 始终显示到屏幕 | 是 | 临时调试，**不要提交到仓库** |

**关键规则**：`DECLARE_LOG_CATEGORY_EXTERN(CategoryName, DefaultVerbosity, CompileTimeVerbosity)`

- `DefaultVerbosity`：**运行时默认**级别，可在 ini 里覆盖（`[Core.Log]` 段）；
- `CompileTimeVerbosity`：**编译期上限**——**低于或等于这个级别的日志才会被编译进二进制**。比如 `CompileTimeVerbosity=Log` 时，你写的所有 `UE_LOG(..., Verbose, ...)` 会被**编译器直接删掉**（零开销），即使 ini 里把级别调到 Verbose 也看不到。

这是新手最常见的困惑："我在 ini 里开了 Verbose，怎么还是不输出？" —— 因为声明时把编译期上限写成了 `Log`。

**本项目的约定**：

```cpp
// 开发期全开（推荐）
DECLARE_LOG_CATEGORY_EXTERN(LogIDFInteraction, Log, All);

// 发布期收窄（等要打包时再改）
DECLARE_LOG_CATEGORY_EXTERN(LogIDFInteraction, Log, Log);
```

### 1.2 `UE_LOG` 与 `UE_LOGFMT`（UE5 的新写法）

```cpp
// 传统写法（仍然完全可用，教程最多）
UE_LOG(LogIDFInteraction, Warning, TEXT("距离过远: %.1f > %.1f"), Dist, MaxDist);

// UE5 结构化日志（编译期检查格式串，推荐用于新代码）
UE_LOGFMT(LogIDFInteraction, Warning, "距离过远: {Dist} > {MaxDist}", ("Dist", Dist), ("MaxDist", MaxDist));
```

**建议**：本节用 `UE_LOG`（资料多、易查），等熟悉后再逐步换 `UE_LOGFMT`。不要在同一个函数里混用两种风格。

### 1.3 日志类别是全局变量（ODR 问题）

```cpp
// 头文件里：
DECLARE_LOG_CATEGORY_EXTERN(LogIDFNet, Log, All);   // 展开成 extern FLogCategoryLogIDFNet LogIDFNet;
extern FLogCategoryLogIDFNet LogIDFNet;             // ← 只是声明，不分配内存

// 某个 .cpp 里（只能有一个）：
DEFINE_LOG_CATEGORY(LogIDFNet);                     // ← 定义，分配内存
```

**已核实（UE5.8）**：这两个宏定义在 `Runtime/Core/Public/Logging/LogCategory.h`（不是很多人以为的 `LogMacros.h`，后者只提供 `UE_LOG` 等宏）。`LogCategory.h` 会被 `CoreMinimal.h` 间接包含，所以正常 include 头文件即可，不用手动 include。

### 1.4 控制台变量（CVar）与控制台命令的区别

这是 UE 里两套不同但互补的机制，**必须分清**：

| | 控制台变量（CVar） | 控制台命令（Console Command） |
|---|---|---|
| 本质 | 一个**有值的**全局状态 | 一个**动作**（函数调用） |
| 声明 | `TAutoConsoleVariable<int32>` / `FAutoConsoleVariableRef` | `FAutoConsoleCommand` 系列 |
| 调用方式 | `IDF.Debug.Interact 1`（**带值**） | `IDF.Debug.DumpState`（**不带值或有多个参数**） |
| 典型用途 | 开关可视化、调参数 | 执行一次性动作（打印状态、传送玩家） |
| 运行时读取 | `CVar.GetValueOnGameThread()` | — |

**判断口诀**：需要**记住一个状态**（1/0、数值）→ CVar；需要**做一件事** → Console Command。

本节两个都要做，各做一个最小例子。

---

## 2. 本节要做的五件事

### 2.1 建模块日志类别头文件

📁 **新建** `Source/Identity5/Identity5Log.h`

✍️ **你要写的代码**：声明主模块与各系统的日志类别。骨架：

```cpp
// Identity5Log.h
#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"   // 提供 UE_LOG 等宏（LogCategory.h 由 CoreMinimal 间接包含）

/**
 * 日志类别命名约定：LogIDF<系统名>
 *  - DefaultVerbosity = Log   ：正常开发时只看关键节点
 *  - CompileTimeVerbosity = All：允许运行时通过 ini 把级别调到 Verbose 而不必重编译
 */
DECLARE_LOG_CATEGORY_EXTERN(LogIdentity5,     Log, All);   // 主模块：生命周期、模块装配
DECLARE_LOG_CATEGORY_EXTERN(LogIDFNet,        Log, All);   // 网络：复制、RPC、预测校正
DECLARE_LOG_CATEGORY_EXTERN(LogIDFAbility,    Log, All);   // GAS：能力激活/失败、标签变化
DECLARE_LOG_CATEGORY_EXTERN(LogIDFAttribute,  Log, All);   // GAS：属性变化与结算
DECLARE_LOG_CATEGORY_EXTERN(LogIDFInteraction,Log, All);   // 交互：扫描、请求、服务器校验
DECLARE_LOG_CATEGORY_EXTERN(LogIDFMatch,      Log, All);   // 对局：阶段、计数、胜负
DECLARE_LOG_CATEGORY_EXTERN(LogIDFAI,         Log, All);   // AI：感知、黑板、行为树
DECLARE_LOG_CATEGORY_EXTERN(LogIDFUI,         Log, All);   // UI：页面栈、ViewModel 绑定
```

⚠️ **注意**：这个头文件放在主模块，但后续 `IDFCore` / `IDMMatch` 插件也需要日志类别。**两种做法**，你要选一种并保持一致：

| 做法 | 说明 | 适用 |
|---|---|---|
| **A（本节采用）** | 所有类别集中在主模块的 `Identity5Log.h`，插件 `PublicDependencyModuleNames` 里加 `Identity5` | 类别少、想统一管理 |
| B | 每个插件在自己的 `Public/` 下建 `IDFCoreLog.h` | 插件要能独立分发、不依赖主模块 |

**注意 A 的隐患**：插件依赖主模块会让"插件独立性"变差（S20 要做"禁用插件不影响其他功能"的验证）。所以我建议**长期按 B 走**，但本节先做 A（因为现在还没有插件），到 S10 建 `IDFCore` 时把 `LogIDFInteraction` 等类别迁到 `Plugins/GameFeatures/IDFCore/Public/IDFCoreLog.h`。

📁 **同时新建** `Source/Identity5/Identity5Log.cpp`

✍️ **你要写的代码**：

```cpp
// Identity5Log.cpp
#include "Identity5Log.h"

DEFINE_LOG_CATEGORY(LogIdentity5);
DEFINE_LOG_CATEGORY(LogIDFNet);
DEFINE_LOG_CATEGORY(LogIDFAbility);
DEFINE_LOG_CATEGORY(LogIDFAttribute);
DEFINE_LOG_CATEGORY(LogIDFInteraction);
DEFINE_LOG_CATEGORY(LogIDFMatch);
DEFINE_LOG_CATEGORY(LogIDFAI);
DEFINE_LOG_CATEGORY(LogIDFUI);
```

✅ **自测**：编译通过。此时还没有任何 `UE_LOG` 使用它们，所以运行看不出区别——但链接器已经不再报 duplicate symbol（因为每个类别只定义了一次）。

---

### 2.2 在模块入口打第一条日志

📁 **修改** `Source/Identity5/Identity5.cpp`

你的文件现在是这样：

```cpp
#include "Identity5.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_PRIMARY_GAME_MODULE( FDefaultGameModuleImpl, Identity5, "Identity5" );
```

✍️ **你要做的改动**：把默认的 `FDefaultGameModuleImpl` 换成**你自己的模块类**，这样才能在模块加载/卸载时打日志。这是理解"UE 模块生命周期"的最好切入点。

```cpp
// Identity5.cpp
#include "Identity5.h"
#include "Identity5Log.h"
#include "Modules/ModuleManager.h"

/**
 * 主游戏模块。
 * UE 的模块有明确的生命周期：StartupModule() 在模块加载时调用（引擎初始化早期），
 * ShutdownModule() 在退出时调用。理解这个时机很重要——例如它比 GameMode 的构造更早，
 * 比世界（World）创建更早，所以这里不能访问 GetWorld()。
 */
class FIdentity5Module : public FDefaultGameModuleImpl
{
public:
    virtual void StartupModule() override
    {
        // FDefaultGameModuleImpl 的 StartupModule 目前是空的，但仍建议调用基类，保持扩展性
        FDefaultGameModuleImpl::StartupModule();

        UE_LOG(LogIdentity5, Log, TEXT("Identity5 主模块已加载（编译时间 %s %s）"), TEXT(__DATE__), TEXT(__TIME__));
    }

    virtual void ShutdownModule() override
    {
        UE_LOG(LogIdentity5, Log, TEXT("Identity5 主模块已卸载"));

        FDefaultGameModuleImpl::ShutdownModule();
    }
};

IMPLEMENT_PRIMARY_GAME_MODULE(FIdentity5Module, Identity5, "Identity5");
```

⚠️ **三个必须点**：

| 项 | 说明 |
|---|---|
| 类要继承 `FDefaultGameModuleImpl` | 它已经处理了 `IMPLEMENT_MODULE` 需要的样板（重载 `StartupModule`/`ShutdownModule`） |
| `IMPLEMENT_PRIMARY_GAME_MODULE(类名, 模块名, "模块名")` | **主游戏模块专用宏**（每个游戏只有一个）。插件用 `IMPLEMENT_MODULE` |
| `__DATE__` / `__TIME__` | 这对宏在**编译时**展开成字符串。它的价值：你能一眼看出"当前运行的 dll 是不是我刚编译的"——排查"我明明改了代码怎么没生效"的神器 |

**为什么这行日志有价值**：编辑器启动时你会看到

```
LogIdentity5: Identity5 主模块已加载（编译时间 Oct  2 2026 17:30:12）
```

以后每次"我改了代码但行为没变"，先看这行的编译时间——能立刻排除"忘了编译/编译失败"这一大类问题。

✅ **自测**：编译 → 打开编辑器 → Output Log 里搜 `LogIdentity5`，能看到这行带编译时间的日志。

---

### 2.3 建调试输出辅助工具

📁 **新建** `Source/Identity5/Identity5Debug.h` 与 `Source/Identity5/Identity5Debug.cpp`

**为什么需要它**：`GEngine->AddOnScreenDebugMessage` 有 6 个参数，还要自己管 Key（防止同一消息刷满屏幕），直接散落在各处调用会很难维护。封装成一个带**节流（throttle）**功能的小工具。

**已核实的准确签名**（`Engine/Classes/Engine/Engine.h:2197`）：

```cpp
void AddOnScreenDebugMessage(uint64 Key, float TimeToDisplay, FColor DisplayColor,
                             const FString& DebugMessage, bool bNewerOnTop = true,
                             const FVector2D& TextScale = FVector2D::UnitVector);
```

**Key 的作用**：同一个 Key 的新消息会**替换**旧消息，而不是叠加。这是"不让屏幕被刷爆"的关键机制。

✍️ **你要写的代码**（骨架，函数体你来写）：

```cpp
// Identity5Debug.h
#pragma once

#include "CoreMinimal.h"

/**
 * 屏幕调试输出的统一入口。
 *
 * 解决的问题：
 *  1. GEngine->AddOnScreenDebugMessage 参数多、Key 需要手工管理，散落各处易重复；
 *  2. 高频调用（例如每帧的交互扫描）会刷爆屏幕，需要节流；
 *  3. 需要统一的"调试开关"控制，避免发布版本残留调试输出。
 */
struct IDENTITY5_API FIDFDebug
{
    /**
     * 显示一条屏幕调试消息。
     * @param Key           消息槽位（同一 Key 会替换旧消息）。建议用稳定常量，不要用随机数
     * @param Message       显示文本
     * @param Duration      持续秒数（用 -1 表示"只显示一帧"，需要每帧调用）
     * @param Color         颜色
     * @param ThrottleSeconds 节流间隔：距离上次**相同 Key** 的调用不足该秒数则跳过。0 表示不节流
     */
    static void Screen(uint64 Key, const FString& Message, float Duration = 0.f,
                       const FColor& Color = FColor::White, float ThrottleSeconds = 0.f);

    /** 清空所有屏幕调试消息 */
    static void ClearScreen();

    /** 总开关：由 CVar 控制（见 Identity5Debug.cpp 中的 IDF.Debug.Enabled） */
    static bool IsEnabled();

private:
    /** Key -> 上次输出时间（秒），用于节流 */
    static TMap<uint64, double> LastPrintTimes;
};
```

⚠️ **实现要点（你来想清楚，我给提示）**：

1. `IsEnabled()` 要读一个 CVar。CVar 的定义放在 `.cpp` 里（文件作用域），例如：
   ```cpp
   static TAutoConsoleVariable<int32> CVarIDFDebugEnabled(
       TEXT("IDF.Debug.Enabled"), 1,
       TEXT("IDF 调试输出总开关：1 开启，0 关闭"),
       ECVF_Cheat);       // ECVF_Cheat 表示"作弊类"，Shipping 构建默认不可改
   ```
2. 节流用 `FPlatformTime::Seconds()` 取当前时间（不要用 `FApp::GetDeltaTime` 累加）；
3. `GEngine` 可能为空（比如在非游戏上下文中调用），必须先判空：
   ```cpp
   if (!GEngine) { return; }
   ```
4. `TMap` 的 Key 用 `uint64`——**用哪个 Key 是调用方的责任**，工具只负责节流。建议后续为每个调试显示项分配固定 Key（例如在 `IDFDebugKeys.h` 里定义枚举）。

✅ **自测**：见 2.5 的自测（需要一个调用点）。

#### 2.3.1 `Identity5Debug.h` 完整实现

```cpp
// Identity5Debug.h
#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"
#include "Engine/EngineBaseTypes.h"   // ENetMode（裸枚举，非 UENUM）

/**
 * 屏幕调试输出的统一入口。
 *
 * 解决三个问题：
 *  1. GEngine->AddOnScreenDebugMessage 参数多、Key 需要手工管理，散落各处容易重复/覆盖；
 *  2. 高频调用（例如每 0.1s 一次的交互扫描）会刷爆屏幕，需要节流；
 *  3. 需要统一的调试总开关，避免发布版本残留调试输出。
 *
 * 注意：本工具依赖 GEngine 与 HUD 绘制，仅在「有渲染的客户端/服务器」有效。
 *       Dedicated Server（-server）上 GEngine 存在但没有视口，屏幕消息不会显示（也不报错）。
 */
struct IDENTITY5_API FIDFDebug
{
    /**
     * 显示一条屏幕调试消息。
     * @param Key              消息槽位（同一 Key 会【替换】旧消息，而不是叠加）。
     *                         建议每个调试显示项使用稳定的常量 Key，不要用随机数或字符串哈希。
     * @param Message          显示文本
     * @param Duration         持续秒数。>0 表示显示这么久；=0 表示只显示这一帧（需每帧调用）；
     *                         <0 表示显示 0 帧但仍会进入消息列表（几乎不用）
     * @param Color            颜色
     * @param ThrottleSeconds  节流间隔（秒）：距上次【相同 Key】的输出不足该秒数则跳过。0 = 不节流
     */
    static void Screen(uint64 Key, const FString& Message, float Duration = 0.f,
                       const FColor& Color = FColor::White, float ThrottleSeconds = 0.f);

    /**
     * 同时输出到屏幕与日志（排查"看不到"问题时最省事的一种）。
     * @param Category  日志类别，例如 LogIDFInteraction
     */
    static void ScreenAndLog(const FLogCategoryBase& Category, FColor Color, uint64 Key,
                             const FString& Message, float Duration = 0.f, float ThrottleSeconds = 0.f);

    /** 清空所有屏幕调试消息（等价于 GEngine->ClearOnScreenDebugMessages()） */
    static void ClearScreen();

    /** 移除某个 Key 的消息 */
    static void RemoveScreen(uint64 Key);

    /** 判断某个 Key 的消息当前是否在屏幕上 */
    static bool ScreenMessageExists(uint64 Key);

    /** 调试总开关：由 CVar "IDF.Debug.Enabled" 控制 */
    static bool IsEnabled();

    /**
     * 把 ENetMode 转成可读字符串。
     * ⚠️ 必须手写：ENetMode 是【裸枚举】（EngineBaseTypes.h:978 的 `enum ENetMode : int`），
     *    没有 UENUM 标记，因此 UEnum::GetValueAsString(World->GetNetMode()) 编译不过。
     */
    static const TCHAR* NetModeToString(ENetMode NetMode);

    /** 取当前网络模式的可读字符串（内部调用 World->GetNetMode()） */
    static FString GetNetModeString(const UWorld* World);

private:
    /** Key -> 上次输出时间（FPlatformTime::Seconds()），用于节流。仅游戏线程访问 */
    static TMap<uint64, double> LastPrintTimes;
};
```


---

#### 2.3.2 `Identity5Debug.cpp` 完整实现（逐段解释）

```cpp
// Identity5Debug.cpp
#include "Identity5Debug.h"

#include "Engine/Engine.h"              // GEngine
#include "Engine/World.h"               // UWorld
#include "HAL/PlatformTime.h"           // FPlatformTime::Seconds()
#include "HAL/IConsoleManager.h"        // TAutoConsoleVariable
#include "Logging/LogCategory.h"        // FLogCategoryBase

// ─────────────────────────────────────────────────────────────
// 1) 静态成员定义
//    TMap 是"每个 Key 上次输出的时间"。因为它是 static，生命周期 = 整个进程，
//    所以 PIE 反复启停不会重置它 —— 这一点对节流无害（时间戳单调递增），
//    但要知道它不会自己清理。条目数 = 你用过的 Key 数量，可忽略不计。
// ─────────────────────────────────────────────────────────────
TMap<uint64, double> FIDFDebug::LastPrintTimes;

// ─────────────────────────────────────────────────────────────
// 2) 调试总开关（CVar）
//    TAutoConsoleVariable 是"带值的变量"：有默认值、可被 ini 覆盖、可在控制台改。
//    ECVF_Cheat：标记为作弊类，Shipping 构建默认不可用（发布版不会留调试后门）。
//    放在文件作用域 = 模块加载时自动注册，不需要手工注册代码。
// ─────────────────────────────────────────────────────────────
static TAutoConsoleVariable<int32> CVarIDFDebugEnabled(
    TEXT("IDF.Debug.Enabled"),
    1,
    TEXT("IDF 调试输出总开关：1 开启，0 关闭（关闭后所有屏幕调试输出静默）"),
    ECVF_Cheat);

bool FIDFDebug::IsEnabled()
{
    // 用 GetValueOnAnyThread() 还是 GetValueOnGameThread()？
    //  - 本项目所有调试输出都在游戏线程调用，用 GetValueOnGameThread() 语义更明确；
    //  - 它的代价是会断言"必须在游戏线程"，能帮你提前发现跨线程误用。
    return CVarIDFDebugEnabled.GetValueOnGameThread() != 0;
}

// ─────────────────────────────────────────────────────────────
// 3) 核心：Screen()
// ─────────────────────────────────────────────────────────────
void FIDFDebug::Screen(uint64 Key, const FString& Message, float Duration,
                       const FColor& Color, float ThrottleSeconds)
{
    // (a) 总开关：关闭时直接返回。注意"先判开关再判节流"，
    //     避免关闭状态下还在维护节流表。
    if (!IsEnabled())
    {
        return;
    }

    // (b) GEngine 可能为空：编辑器关闭游戏视口、命令行工具、命令行 cook 等上下文。
    //     这是最常见的崩溃点，必须最先判空。
    if (GEngine == nullptr)
    {
        return;
    }

    // (c) 节流：只有 ThrottleSeconds > 0 且"相同 Key 距上次输出不足间隔"时才跳过。
    //     时间基准用 FPlatformTime::Seconds()（单调递增的高精度秒），
    //     不要用 FApp::GetDeltaTime() 累加 —— 会随帧率漂移，也不该用于"时间点"比较。
    if (ThrottleSeconds > 0.f)
    {
        const double Now = FPlatformTime::Seconds();
        if (double* LastTime = LastPrintTimes.Find(Key))
        {
            if (Now - *LastTime < static_cast<double>(ThrottleSeconds))
            {
                return;   // 未到下一次允许输出的时间，静默跳过
            }
        }
        LastPrintTimes.Add(Key, Now);   // 首次或已到期：记录本次输出时间
    }

    // (d) 真正输出。
    //     参数语义（已核实 Engine/Classes/Engine/Engine.h:2197）：
    //       Key            —— 同一 Key 的消息会【替换】，不是叠加；
    //       TimeToDisplay  —— 显示时长（秒）；0 = 只显示本帧；
    //       bNewerOnTop    —— 【仅在 Key == INDEX_NONE 时生效】（引擎注释明确写了这一点）。
    //                         我们始终传固定 Key，所以这个参数对我们没有实际作用，传默认值即可。
    GEngine->AddOnScreenDebugMessage(Key, Duration, Color, Message);
}

// ─────────────────────────────────────────────────────────────
// 4) ScreenAndLog()：屏幕 + 日志双通道
//    ⚠️ 模板定义必须放在头文件才能被其他翻译单元使用；放在 .cpp 里只能本文件用。
//       所以这里【不用模板】，而是用 UE_LOG 的格式化入口 + FString 拼接。
// ─────────────────────────────────────────────────────────────
void FIDFDebug::ScreenAndLog(const FLogCategoryBase& Category, FColor Color, uint64 Key,
                             const FString& Message, float Duration, float ThrottleSeconds)
{
    // 屏幕部分
    Screen(Key, Message, Duration, Color, ThrottleSeconds);

    // 日志部分：UE_LOG 的第一个参数要求是"日志类别【变量名】"（宏内部用 #Category 取名字），
    // 因此无法把 FLogCategoryBase& 这种变量传进去，只能走底层入口 FMsg::Logf。
    //
    // 已核实签名（Runtime/Core/Public/Logging/LogMacros.h:62）：
    //   static void Logf(const ANSICHAR* File, int32 Line, const FLogCategoryName& Category,
    //                    ELogVerbosity::Type Verbosity, const FmtType& Fmt, Types... Args)
    // 两个要点：
    //   1) File 形参是 ANSICHAR*，所以 __FILE__（窄字符）直接传，【不要】套 TEXT()；
    //   2) Category 形参是 FLogCategoryName（UE5.8 中为 FLazyName 或 FName 的条件别名，
    //      见 LogCategory.h:13/16），而 GetCategoryName() 返回的正是 FLogCategoryName&，
    //      类型直接匹配，无需转换。
    FMsg::Logf(__FILE__, __LINE__, Category.GetCategoryName(),
               ELogVerbosity::Log, TEXT("%s"), *Message);
}

// ─────────────────────────────────────────────────────────────
// 5) 其余小工具
// ─────────────────────────────────────────────────────────────
void FIDFDebug::ClearScreen()
{
    if (GEngine)
    {
        GEngine->ClearOnScreenDebugMessages();
    }
    LastPrintTimes.Reset();   // 顺手清掉节流表，避免"清了屏幕却要等节流到期"
}

void FIDFDebug::RemoveScreen(uint64 Key)
{
    if (GEngine)
    {
        GEngine->RemoveOnScreenDebugMessage(Key);
    }
    LastPrintTimes.Remove(Key);
}

bool FIDFDebug::ScreenMessageExists(uint64 Key)
{
    // OnScreenDebugMessageExists 需要 GEngine；无引擎时视为不存在
    return GEngine != nullptr && GEngine->OnScreenDebugMessageExists(Key);
}

// ─────────────────────────────────────────────────────────────
// 6) ENetMode → 字符串
//    ⚠️ ENetMode 是裸枚举（EngineBaseTypes.h:978: `enum ENetMode : int`），
//       没有 UENUM 标记，因此 UEnum::GetValueAsString() 用不了 —— 必须手写 switch。
//       这是"看起来该有现成 API、实际没有"的典型情况，写错就是编译错误。
// ─────────────────────────────────────────────────────────────
const TCHAR* FIDFDebug::NetModeToString(ENetMode NetMode)
{
    switch (NetMode)
    {
    case NM_Standalone:       return TEXT("Standalone");
    case NM_DedicatedServer:  return TEXT("DedicatedServer");
    case NM_ListenServer:     return TEXT("ListenServer");
    case NM_Client:           return TEXT("Client");
    default:                  return TEXT("Unknown");
    }
}

FString FIDFDebug::GetNetModeString(const UWorld* World)
{
    if (World == nullptr)
    {
        return TEXT("<无 World>");
    }
    return FString(NetModeToString(World->GetNetMode()));
}
```

**逐段要点回顾**（面试会问的就是这些"为什么"）：

| 代码 | 为什么这么写 |
|---|---|
| `static TMap<uint64,double> LastPrintTimes` | 节流必须记住"上次什么时候输出的"；用 `double` 是因为 `FPlatformTime::Seconds()` 返回 `double` |
| 先判 `IsEnabled()` 再判 `ThrottleSeconds` | 关闭状态下不该继续维护状态，也不该做无谓计算 |
| `if (GEngine == nullptr) return;` | 编辑器关视口、命令行工具等上下文里 `GEngine` 为空，是最常见的崩溃点 |
| `FPlatformTime::Seconds()` | 单调递增的高精度秒；`FApp::GetDeltaTime()` 是"帧间隔"，用来比较时间点会随帧率漂移 |
| `Duration = 0` 表示只显示本帧 | 引擎语义。所以"常驻显示"必须**每帧调用**，配合固定 Key 覆盖 —— 这正好和"节流"配套使用 |
| `bNewerOnTop` 我们不用 | 引擎注释明确写它只在 `Key == INDEX_NONE` 时生效；我们总用固定 Key |
| `ScreenAndLog` 不能做成模板 | 模板定义放 `.cpp` 只对本文件可见；而 `UE_LOG` 的第一个参数必须是**类别变量名**（宏内部用 `#Category`），无法接受变量，所以走 `FMsg::Logf` |

⚠️ **关于 `Category.GetCategoryName()`（我在引擎里核实后的修正）**：`FLogCategoryName` 在 UE5.8 中是**条件别名**——`LogCategory.h:13` 定义为 `FLazyName`，`:16` 定义为 `FName`（取决于构建配置）。所以：

- ✅ `Category.GetCategoryName()` 直接传给 `FMsg::Logf` 的第 3 个形参**类型完全匹配**；
- ❌ 不要写 `Category.GetCategoryName() != NAME_None` 这类与 `FName` 绑定的比较——在 `FLazyName` 配置下不可移植（我最初就是这么写的，已修正）。

⚠️ **`FMsg::Logf` 的注意点**：它是引擎内部使用的通用日志入口（`Logging/LogMacros.h` 里 `UE_LOG` 最终也会走到类似路径）。它的签名要求传**文件名、行号、类别名、verbosity、格式串**。这里用 `__FILE__` / `__LINE__` 传的是 `Identity5Debug.cpp` 自身的位置——如果你希望日志定位到**调用方**，就该改成在头文件里用宏包装（例如定义 `IDF_DEBUG_LOG(Category, ...)` 宏，把 `__FILE__`/`__LINE__` 在调用点展开）。

**如果你觉得 `FMsg::Logf` 太底层，有个更简单的替代**：把 `ScreenAndLog` 改成宏，或者干脆让调用方自己写两行：

```cpp
// 简单直接，代价是调用点写两行
FIDFDebug::Screen(Key::Interact, Msg, 0.f, FColor::Yellow, 0.5f);
UE_LOG(LogIDFInteraction, Verbose, TEXT("%s"), *Msg);
```

**我的建议**：本阶段先用后者（简单、无坑），等 S11 真的需要大量"屏幕+日志"双输出时，再回头把 `ScreenAndLog` 做成头文件里的宏。**不要为了统一而引入你还没完全理解的底层 API**。

✅ **自测**：
1. 编译通过（注意 `ENetMode` 的 switch 必须覆盖 `NM_Client`，否则会有编译警告 `not all enumeration values handled`）；
2. 在某个 `Tick` 里调用 `FIDFDebug::Screen(100, TEXT("test"), 0.f, FColor::Green, 1.f)` → 屏幕显示且每秒只刷新一次；
3. `IDF.Debug.Enabled 0` → 屏幕输出立即消失；`IDF.Debug.Enabled 1` → 恢复；
4. 调用 `FIDFDebug::ClearScreen()` → 屏幕清空（含节流表重置）。

### 2.4 做控制台命令与控制台变量

📁 **新建** `Source/Identity5/Identity5DebugCommands.cpp`

**已核实的类**（`HAL/IConsoleManager.h:2411`）：

```cpp
FAutoConsoleCommandWithWorldAndArgs(const TCHAR* Name, const TCHAR* Help,
                                    const FConsoleCommandWithWorldAndArgsDelegate& Command,
                                    uint32 Flags = ECVF_Default);
```

**委托签名**（`HAL/IConsoleManager.h:257` —— 参数顺序是「Args 在前、World 在后」）：

```cpp
DECLARE_DELEGATE_TwoParams(FConsoleCommandWithWorldAndArgsDelegate, const TArray<FString>&, UWorld*);
```

所以 lambda 必须写成 `[](const TArray<FString>& Args, UWorld* World)`。带 `World*` 的价值：省掉"手动从 `GEngine` 拿 World"的样板，且在 World 已销毁时会是 `nullptr`（可以安全判断）。

#### 2.4.1 完整实现

```cpp
// Identity5DebugCommands.cpp
//
// IDF 调试控制台入口。
//
// 设计要点：
//  1) 所有 FAutoConsoleCommand* / TAutoConsoleVariable / FAutoConsoleVariableRef 都声明为
//     文件作用域【静态对象】—— 它们在模块加载时构造、卸载时析构，
//     构造函数内部就是"注册命令"的动作，所以不需要任何手工注册代码。
//  2) 命令名统一 IDF. 前缀，控制台里输入 "IDF." 会列出全部。
//  3) 关于 Flags：ECVF_Cheat 的语义是"在最终发布版中隐藏、用户无法修改"，
//     开发期（编辑器/PIE/Development）一切正常。所以这里统一用 ECVF_Cheat，
//     将来真要发布时无需改代码。若你在 PIE 里发现命令找不到，把它改成 ECVF_Default
//     再试一次即可（这一步能帮你确认问题是否出在 Flags 上）。

#include "CoreMinimal.h"

#include "Identity5Log.h"
#include "Identity5Debug.h"

#include "Engine/Engine.h"        // GEngine
#include "Engine/World.h"         // UWorld / GetPlayerControllerIterator
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

#include "HAL/IConsoleManager.h"

// ═══════════════════════════════════════════════════════════════
// 控制台变量（CVar）：有值、表示状态
// ═══════════════════════════════════════════════════════════════

// ① 用 FAutoConsoleVariableRef 把"一个已有变量"暴露成 CVar。
//    适合：已经有一个 int32/bool/float 成员，想让它可被控制台修改。
static int32 GIDFDrawDebugInfo = 0;
static FAutoConsoleVariableRef CVarIDFDrawDebugInfo(
    TEXT("IDF.Debug.Draw"),
    GIDFDrawDebugInfo,
    TEXT("是否在屏幕上绘制 IDF 调试信息：0 关闭，1 开启"),
    ECVF_Cheat);

// 供其它系统查询（后续 S11 可视化交互点、S13 显示破译进度都会用到）。
// 放在头文件里声明、这里定义，避免各处直接访问 GIDFDrawDebugInfo。
bool IDFDebug_ShouldDrawDebugInfo()
{
    return GIDFDrawDebugInfo != 0;
}

// ② 用 TAutoConsoleVariable 创建一个"自带存储"的 CVar。
//    适合：值只被内部使用，不需要暴露成普通变量。
//    读取方式与 ① 不同：要调用 GetValueOnGameThread()。
static TAutoConsoleVariable<int32> CVarIDFDebugVerboseLog(
    TEXT("IDF.Debug.VerboseLog"),
    0,
    TEXT("是否把 IDF 调试信息同时写入日志：0 关闭，1 开启"),
    ECVF_Cheat);

static bool IDFDebug_ShouldVerboseLog()
{
    return CVarIDFDebugVerboseLog.GetValueOnGameThread() != 0;
}

// ═══════════════════════════════════════════════════════════════
// 控制台命令：一个动作
// ═══════════════════════════════════════════════════════════════

// ③ 最小验证命令：带参数
static FAutoConsoleCommandWithWorldAndArgs CmdIDFDebugHello(
    TEXT("IDF.Debug.Hello"),
    TEXT("验证调试基础设施：在屏幕与日志中输出一条消息。用法：IDF.Debug.Hello [文本...]"),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
        [](const TArray<FString>& Args, UWorld* World)
        {
            // ⚠️ 不要用 FString::Join —— 该 API 在 UE5.8 中【不存在】
            //    （FString 所在的 Containers/UnrealString.h 里没有 Join，只有 FText::Join，
            //      且 FText::Join 的参数顺序是 (Delimiter, Args)，很容易记错）。
            //    这里用手写拼接，最直观也最稳。
            FString Text;
            for (int32 Index = 0; Index < Args.Num(); ++Index)
            {
                if (Index > 0)
                {
                    Text.AppendChar(TEXT(' '));
                }
                Text.Append(Args[Index]);
            }
            if (Text.IsEmpty())
            {
                Text = TEXT("Hello, Identity5");
            }

            UE_LOG(LogIdentity5, Log, TEXT("IDF.Debug.Hello 被调用：%s"), *Text);
            FIDFDebug::Screen(1, Text, 5.f, FColor::Cyan);
        }));

// ④ "转储世界状态"命令：需要遍历世界（S08 做阵营分配时的主力验证工具）
static FAutoConsoleCommandWithWorldAndArgs CmdIDFDumpWorld(
    TEXT("IDF.Debug.DumpWorld"),
    TEXT("打印当前世界的调试摘要：地图、网络模式、玩家、Pawn。"),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
        [](const TArray<FString>& Args, UWorld* World)
        {
            if (World == nullptr)
            {
                UE_LOG(LogIdentity5, Warning, TEXT("IDF.Debug.DumpWorld：没有有效的 World"));
                return;
            }

            UE_LOG(LogIdentity5, Log, TEXT("========== IDF 世界摘要 =========="));
            UE_LOG(LogIdentity5, Log, TEXT("地图名      : %s"), *World->GetMapName());
            UE_LOG(LogIdentity5, Log, TEXT("网络模式    : %s"), *FIDFDebug::GetNetModeString(World));
            UE_LOG(LogIdentity5, Log, TEXT("玩家控制器数: %d"), World->GetNumPlayerControllers());

            // 遍历玩家。
            // 已核实（Engine/Classes/Engine/World.h:93 与 :2764）：
            //   FConstPlayerControllerIterator = TArray<TWeakObjectPtr<APlayerController>>::TConstIterator
            // 因此解引用得到的是【弱指针】，必须 Get() 并判空。
            int32 PlayerIndex = 0;
            for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
            {
                APlayerController* PC = It->Get();
                if (PC == nullptr)
                {
                    UE_LOG(LogIdentity5, Warning, TEXT("  [%d] PlayerController 已失效（弱指针为空）"), PlayerIndex);
                    ++PlayerIndex;
                    continue;
                }

                const APawn* Pawn = PC->GetPawn();
                const APlayerState* PS = PC->PlayerState;

                UE_LOG(LogIdentity5, Log, TEXT("  [%d] 控制器类=%s  本地控制=%s  PlayerState=%s  Pawn=%s"),
                    PlayerIndex,
                    *GetNameSafe(PC->GetClass()),
                    PC->IsLocalController() ? TEXT("是") : TEXT("否"),
                    PS ? *GetNameSafe(PS->GetClass()) : TEXT("<无>"),
                    Pawn ? *GetNameSafe(Pawn->GetClass()) : TEXT("<无 Pawn>"));

                // Pawn 的位置对排查"生成在哪里"很有用
                if (Pawn != nullptr)
                {
                    const FVector Loc = Pawn->GetActorLocation();

                    // ⚠️ 注意：APlayerState::GetPlayerName() 是【按值返回 FString】。
                    //    若写成 *PS->GetPlayerName()，虽然在同一表达式内临时对象尚未析构、
                    //    实际能跑通，但非常脆弱（且容易被误改成跨语句使用）。这里先取到局部变量。
                    const FString PlayerName = PS ? PS->GetPlayerName() : FString(TEXT("<未知>"));

                    UE_LOG(LogIdentity5, Log, TEXT("       位置=(%.0f, %.0f, %.0f)  所属玩家=%s"),
                        Loc.X, Loc.Y, Loc.Z, *PlayerName);
                }

                ++PlayerIndex;
            }

            UE_LOG(LogIdentity5, Log, TEXT("=================================="));
        }));

// ⑤ 清屏命令：只做一件事，无参数
static FAutoConsoleCommand CmdIDFDebugClear(
    TEXT("IDF.Debug.Clear"),
    TEXT("清空所有 IDF 屏幕调试消息。"),
    FConsoleCommandDelegate::CreateLambda(
        []()
        {
            FIDFDebug::ClearScreen();
            UE_LOG(LogIdentity5, Log, TEXT("IDF 屏幕调试消息已清空"));
        }));

// ⑥ 打印调试开关当前状态（验证 CVar 是否真的生效，比在控制台里逐个查询省事）
static FAutoConsoleCommand CmdIDFDebugStatus(
    TEXT("IDF.Debug.Status"),
    TEXT("打印 IDF 调试开关的当前状态。"),
    FConsoleCommandDelegate::CreateLambda(
        []()
        {
            UE_LOG(LogIdentity5, Log, TEXT("=== IDF 调试开关 ==="));
            UE_LOG(LogIdentity5, Log, TEXT("  IDF.Debug.Enabled    = %d"), FIDFDebug::IsEnabled() ? 1 : 0);
            UE_LOG(LogIdentity5, Log, TEXT("  IDF.Debug.Draw       = %d"), GIDFDrawDebugInfo);
            UE_LOG(LogIdentity5, Log, TEXT("  IDF.Debug.VerboseLog = %d"), IDFDebug_ShouldVerboseLog() ? 1 : 0);
            UE_LOG(LogIdentity5, Log, TEXT("  编译时间             = %s %s"), TEXT(__DATE__), TEXT(__TIME__));
        }));
```

#### 2.4.2 五个必须理解的点

| 点 | 说明 |
|---|---|
| **静态对象即注册** | `static FAutoConsoleCommandXxx CmdXxx(...)` 在模块加载时构造 → 构造函数调用 `IConsoleManager::RegisterConsoleCommand` → 命令生效。**不需要** `StartupModule` 里手工注册。这也意味着：**它必须放在文件作用域**，放函数里会在函数返回时析构 = 命令立刻注销 |
| **不要用 `FString::Join`** | **UE5.8 的 `FString` 没有 `Join`**（我核实过 `Containers/UnrealString.h`）。只有 `FText::Join(Delimiter, Args)`，参数顺序与直觉相反。拼字符串请用手写循环或 `FString::Printf` |
| **`Args[0]` 是第一个参数** | 命令名不在 `Args` 里。`IDF.Debug.Hello a b` → `Args = {"a", "b"}`。访问前务必 `Args.IsValidIndex(i)` 或先判 `Args.Num()` |
| **两种 CVar 记法要分清** | `FAutoConsoleVariableRef` 是"把已有变量暴露出去"，读取时**直接用变量名**（`GIDFDrawDebugInfo`）；`TAutoConsoleVariable` 是"自带存储"，读取要 `GetValueOnGameThread()`。混用会编译不过 |
| **`GetNameSafe` 而不是 `GetName`** | `GetNameSafe(Ptr)` 对 `nullptr` 返回 `"None"`，`Ptr->GetName()` 会崩。日志代码里一律用 `GetNameSafe` |

#### 2.4.3 `ECVF_Cheat` 的语义（已核实）

引擎注释原文（`HAL/IConsoleManager.h:72-76`）：

> *Console variables marked with this flag behave differently in a final release build. Then they are hidden in the console and cannot be changed by the user.*

所以：**开发期完全正常，只在最终发布版隐藏**。这就是为什么正式项目都用它——发布时不需要改代码。

⚠️ 如果你在 PIE 里发现 `IDF.` 命令**找不到**，排查顺序：
1. 该 `.cpp` 是否被编译进去（看 `Binaries/Win64/UnrealEditor-Identity5.dll` 时间戳，或搜 `Identity5DebugCommands` 是否出现在编译输出里）；
2. 是否放在了函数内而非文件作用域；
3. 把 Flags 从 `ECVF_Cheat` 临时改成 `ECVF_Default` 再试（用于区分"Flags 问题"与"注册问题"）。

#### 2.4.4 自测（本节最重要的验证）

1. 编译 → PIE（**Standalone 和 Listen Server 都要试**）；
2. 按 `~` 输入 `IDF.` → 应看到 6 个入口（Hello / DumpWorld / Clear / Status + Draw / VerboseLog 两个 CVar）的自动补全；
3. `IDF.Debug.Hello 你好` → 屏幕出现青色文字 + 日志出现记录；
4. `IDF.Debug.Draw 1` → 再输入 `IDF.Debug.Draw`（不带值）应回显 `= 1`；
5. `IDF.Debug.Status` → 日志打印各开关状态；
6. `IDF.Debug.DumpWorld` → 打印地图名、网络模式、玩家列表、每个 Pawn 的类与位置；
7. **对比服务器与客户端**：在 Listen Server 的客户端窗口执行第 6 项，网络模式应显示 `Client`；服务器窗口显示 `ListenServer`。同时观察"本地控制"列——**客户端窗口里只有它自己那一个是"是"**，服务器窗口里则取决于是否有本地玩家（Listen Server 有）。


---

### 2.5 配置默认日志级别

📁 **修改** `Config/DefaultEngine.ini`，追加：

```ini
; ══════════════════════════════════════════════════════════════
; 日志类别默认显示级别
; 语法（已核实 Engine/Config/BaseEngine.ini:1642 的官方说明）：
;   [Core.Log]
;   LogCategoryName=Verbosity
; 可用级别：NoLogging / Fatal / Error / Warning / Log / Verbose / VeryVerbose / All
;
; 注意：这里只能"放宽"到声明时的 CompileTimeVerbosity 允许的范围内。
;       若某类别的 CompileTimeVerbosity=Log，这里写 Verbose 也无效（代码里根本没编译进去）。
; ══════════════════════════════════════════════════════════════
[Core.Log]
LogIdentity5=Log
LogIDFNet=Log
LogIDFAbility=Log
LogIDFAttribute=Log
LogIDFInteraction=Verbose
LogIDFMatch=Log
LogIDFAI=Log
LogIDFUI=Log
```

**为什么 `LogIDFInteraction=Verbose` 单独放宽**：交互扫描每 0.1s 一次，属于高频噪音，开发期用 `Verbose` 排查最方便，但又不想让它在默认级别下刷屏——`Verbose` 级别正好：只有你主动调高时才输出。

**运行时动态调整**（不用改 ini、不用重启）：控制台输入

```
Log LogIDFInteraction Verbose
Log LogIDFNet VeryVerbose
```

⚠️ 常见坑：这个命令作用于**当前实例**（PIE 里就是当前那个客户端窗口）。5 个窗口要分别设置。

---

## 3. 自测清单（本节 DoD）

| # | 验证项 | 期望结果 |
|---|---|---|
| 1 | 编译 | 无 Error；`Binaries/Win64/UnrealEditor-Identity5.dll` 时间戳已更新 |
| 2 | 模块日志 | Output Log 搜 `LogIdentity5`，看到带 `__DATE__ __TIME__` 的加载日志 |
| 3 | 日志分类可见 | Output Log 的过滤器下拉里能看到 `LogIDFInteraction` 等类别 |
| 4 | 命令注册 | 控制台输入 `IDF.` 有自动补全，共 3 个命令/变量 |
| 5 | 命令执行 | `IDF.Debug.Hello 测试` 屏幕+日志都有输出 |
| 6 | CVar 有状态 | `IDF.Debug.Draw 1` 后查询回显 1；`IDF.Debug.Draw 0` 后回显 0 |
| 7 | World 命令 | `IDF.Debug.DumpWorld` 打印地图名与网络模式 |
| 8 | **客户端 vs 服务器** | 在 Listen Server 的客户端窗口执行第 7 项，网络模式显示 `Client`；服务器窗口显示 `ListenServer`。同时看"本地控制"列：客户端窗口里只有它自己为"是" |
| 9 | ini 生效 | 把 `LogIDFInteraction` 改成 `Warning`，重启编辑器后 Verbose 日志消失（验证 `[Core.Log]` 真被读取） |
| 10 | 节流有效 | 在 `Tick` 里连续调用 `FIDFDebug::Screen` 并设 `ThrottleSeconds=1`，屏幕文字每秒只刷新一次、不叠加 |

---

## 4. 常见坑汇总

| 现象 | 原因 | 处理 |
|---|---|---|
| 链接错误 `duplicate symbol LogXXX` | `DEFINE_LOG_CATEGORY` 写了两次 | 全工程只留一处定义（其余用 `DECLARE_LOG_CATEGORY_EXTERN`） |
| `unresolved external symbol LogXXX` | 只声明没定义（或定义所在的 cpp 没被编译） | 检查 `DEFINE_LOG_CATEGORY` 是否存在 |
| ini 里调到 `Verbose` 但仍不输出 | 声明时 `CompileTimeVerbosity` 写成了 `Log` | 改成 `All` |
| 控制台找不到命令 | 命令对象放在函数内 / 模块未加载 / 拼写错误 | 必须是文件作用域静态对象；命令名大小写敏感 |
| `GEngine` 访问崩溃 | 在非游戏上下文（如命令行工具）中使用 | 所有 `GEngine` 使用前判空 |
| 屏幕消息刷爆 | 每条消息用了不同的 Key | 用固定 Key；同一类信息共用一个 Key |
| `Log LogIDFNet Verbose` 无效 | 你改的是服务器窗口，但日志来自客户端（或反之） | 每个窗口分别设置 |
| 编译报 `FString::Join` 不存在 | **UE5.8 的 FString 没有 Join**（只有 `FText::Join(Delimiter, Args)`，参数顺序相反） | 手写循环拼接，或 `FString::Printf` |
| 编译报 `ENetMode` 参数类型不匹配（`UEnum::GetValueAsString`） | `ENetMode` 是**裸枚举**，不是 UENUM | 用 `FIDFDebug::NetModeToString()` 手写 switch（见 2.3.2） |
| 遍历玩家时崩溃 | `FConstPlayerControllerIterator` 解引用得到的是**弱指针** `TWeakObjectPtr<APlayerController>` | 用 `It->Get()` 并判空 |
| `FColor` 直接用 `if (Color)` 判断编译失败 | `FColor` 是**结构体**，不是枚举，没有 bool 转换 | 用显式比较，例如 `Color != FColor::White` |
| `__DATE__`/`__TIME__` 不变 | 该 cpp 没有重新编译（UBT 只编译改动过的文件） | 正常现象；改这个文件就会更新。想看"是否最新"，改一下文件再编译 |
| 发布构建里调试命令消失了 | `ECVF_Cheat` 的语义就是"最终发布版中隐藏" | 这是**预期行为**，不是 bug |

---

## 5. 提交

```powershell
cd 'G:\Project\Unreal Projects\Identity5'

git add Source/Identity5/Identity5Log.h Source/Identity5/Identity5Log.cpp `
        Source/Identity5/Identity5Debug.h Source/Identity5/Identity5Debug.cpp `
        Source/Identity5/Identity5DebugCommands.cpp `
        Source/Identity5/Identity5.cpp Config/DefaultEngine.ini

git commit -m "feat(debug): 建立分层日志类别、屏幕调试输出与 IDF.* 控制台命令/CVar"
```

> 注意 `git add` 里**没有** `Binaries/`（被忽略）——这是正确的，构建产物不该进仓库。

---

## 6. 简历点与面试问答

💼 **可以这样写**：

> **可观测性基础设施**：按系统划分日志类别（网络/能力/属性/交互/对局/AI/UI 共 8 个），区分运行时默认级别与编译期上限（`CompileTimeVerbosity` 控制 Verbose 日志是否进二进制）；封装带节流的屏幕调试输出；提供 `IDF.*` 控制台命令与 CVar 开关，支持运行时无重编译排查。

**可能被追问的问题，先想清楚答案**：

1. **"一个日志类别能控制多少？为什么不全用 `LogTemp`？"**
   → 类别决定过滤粒度与运行时级别。全用 `LogTemp` 就无法按模块开关，也无法在打包时按模块裁掉 Verbose（编译期上限是按类别设置的）。

2. **"CVar 和 Console Command 有什么区别？各举一个本项目的例子。"**
   → CVar 有值、表示状态（`IDF.Debug.Draw 1` 开关可视化）；Console Command 是动作（`IDF.Debug.DumpWorld` 打印一次状态）。选择依据是"要不要记住一个状态"。

3. **"为什么这些命令在 Shipping 版本里要禁用？"**
   → 它们是调试后门（`ECVF_Cheat`）。打包发布时若仍可通过控制台开启，等于给玩家开了调试权限；另外 Shipping 会裁掉相关代码减小体积。

4. **"你怎么排查'技能点了没反应'这类没有任何报错的问题？"**
   → 这是最能体现工程能力的问题。答案框架：先把 `LogIDFAbility` 调到 `Verbose`，确认"输入事件是否到达能力系统"→"`CanActivateAbility` 是否被拒绝以及拒绝原因（标签阻断/冷却/成本）"→"`CommitAbility` 是否成功"→ 再看属性/标签复制。**每一环都有日志埋点**，这就是本节的价值。

---

## 7. 下一步

S04 完成后就可以进 **S05：最小 GAS 骨架**了。在此之前，建议你把 `IDF.Debug.DumpWorld` 的 TODO 部分补完（遍历打印每个 PlayerController/Pawn/PlayerState）——S08 做阵营分配时，这个命令会成为你最主要的验证工具，现在写好能省后面的事。
