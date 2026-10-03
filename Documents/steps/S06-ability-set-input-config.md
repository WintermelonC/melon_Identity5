# S06 实施步骤：AbilitySet + InputConfig + 原生 GameplayTag

> 对应 `implementation_roadmap.md` 的 S06。**目标**：把"角色能力装配"变成**数据驱动**——一份数据资产（`UIDFAbilitySet`）声明"这个角色有哪些能力、哪些初始效果、哪些属性集"，另一份（`UIDFInputConfig`）声明"哪个输入触发哪个能力"；同时把所有 GameplayTag 用 **C++ 原生方式**注册，彻底告别裸字符串。
> 本节预计 4~6 小时。
> **所有新文件都在 `IDFCore` 插件里**（不再动主模块），导出宏一律 `IDFCORE_API`。

---

## 0. 本节要建立的四个正确认知

### 认知 1：为什么要有 AbilitySet 这一层？直接 `GiveAbility` 不行吗？

可以，但会迅速失控。对比一下两种写法：

```cpp
// ❌ 散落式：每个角色的 BeginPlay 里一长串硬编码
if (IsDoctor())
{
    AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(UGA_Doctor_SelfHeal::StaticClass()));
    AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(UGA_Survivor_Heal::StaticClass()));
    AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(UGA_Survivor_Rescue::StaticClass()));
    // ... 20 行
}
else if (IsAirman()) { /* 又是 20 行 */ }

// ✅ 数据驱动：一份数据资产 + 一行调用
DefaultAbilitySet->GiveToAbilitySystem(AbilitySystemComponent);
```

**AbilitySet 解决的三个实际问题**：

| 问题 | 数据驱动后 |
|---|---|
| 加一个技能要改 C++ 并重编译 | 在编辑器里给数据资产加一行 |
| 能力与"输入标签"的对应关系散落各处 | 在同一个资产里成对声明 |
| 无法在设计阶段一眼看到"这个角色有哪些能力" | 打开资产就能看全 |

**一个关键细节**：`GiveToAbilitySystem` 必须做**幂等**——它在服务器和客户端都会被调用（S07 的双端初始化），重复授予会导致能力列表里出现多份。

### 认知 2：原生 GameplayTag 的宏有"内外分工"，不能乱用

我核实了 `NativeGameplayTags.h` 的宏定义（`GameplayTags` 插件，`Runtime/GameplayTags/Public/NativeGameplayTags.h`）：

| 宏 | 用在哪 | 展开 |
|---|---|---|
| `UE_DECLARE_GAMEPLAY_TAG_EXTERN(TagName)` | **头文件**（`.h`） | `extern FNativeGameplayTag TagName;`（只有声明） |
| `UE_DEFINE_GAMEPLAY_TAG(TagName, "Tag.Path")` | **`.cpp`**（只能一处） | 定义对象，构造时向引擎注册 |
| `UE_DEFINE_GAMEPLAY_TAG_STATIC(...)` | `.cpp` 里、只给本文件用 | `static FNativeGameplayTag ...` |
| `UE_DEFINE_GAMEPLAY_TAG_COMMENT(...)` | `.cpp`，带开发者注释 | 同上 + 注释 |

**宏里有 `static_assert` 强制检查**（我核实过源码）：

```cpp
static_assert(UE::GameplayTags::Private::HasFileExtension(__FILE__),
    "UE_DEFINE_GAMEPLAY_TAG can only be used in .cpp files, "
    "if you're trying to share tags across modules, use UE_DECLARE_GAMEPLAY_TAG_EXTERN "
    "in the public header, and UE_DEFINE_GAMEPLAY_TAG in the private .cpp");
```

也就是说：**把 `UE_DEFINE_GAMEPLAY_TAG` 写进头文件会直接编译失败**，而且报错信息会明确告诉你正确做法。这是引擎在帮你防"每个 include 它的文件都定义一次 → 重复符号"。

**注册机制**：`FNativeGameplayTag` 的构造函数在**静态初始化阶段**向 `UGameplayTagsManager` 注册标签，模块卸载时反注册。所以：

- **模块加载时标签就存在了**，不需要任何 `StartupModule` 里的手工代码；
- 这也是它比 ini 标签表更适合代码引用标签的原因（无字符串查找、编译期检查拼写）。

### 认知 3：项目的插件名是 `Identity5`，模块名是 `IDFCore`——两者不同

`UE_DEFINE_GAMEPLAY_TAG` 展开时会把 `UE_PLUGIN_NAME` 和 `UE_MODULE_NAME` 一起注册进标签元数据。在本项目里：

- `UE_PLUGIN_NAME` = **`Identity5`**（工程名，不是插件名）；
- `UE_MODULE_NAME` = **`IDFCore`**。

引擎的取舍规则是"模块名就是插件名时用插件名，否则优先插件名"（`UE_PLUGIN_NAME != UE_MODULE_NAME` 时取插件名），所以这些标签在编辑器里会显示为 **`Identity5`** 来源，而不是 `IDFCore`。**这不影响任何功能**，只是标签浏览器的来源列不同——**不要求修正**，知道就行。

### 认知 4：`FGameplayAbilitySpec` 的输入绑定：`InputID` 已过时，用标签

```cpp
// ❌ 老写法（UE4 时代）：用数字 InputID
FGameplayAbilitySpec(AbilityClass, Level, /*InputID=*/0);

// ✅ 推荐：能力自己声明"我响应哪个输入标签"，由 ASC 按标签转发
FGameplayAbilitySpec(AbilityClass, Level, INDEX_NONE, SourceObject);
// 然后在能力里设置 AbilityTags 或动态标签，ASC 用 GetDynamicSpecSourceTags() 匹配输入
```

我核实过：`FGameplayAbilitySpec` 的第 3 个参数仍叫 `InInputID`，但 `DynamicAbilityTags` 已被标记 **`UE_DEPRECATED(5.5, ...)`**，替代品是 `GetDynamicSpecSourceTags()`（`GameplayAbilitySpec.h:242-249`）。

**本项目的约定**：`UIDFAbilitySet` 里的 `InputTag` 字段在授予能力时通过 `Spec.GetDynamicSpecSourceTags().AddTag(InputTag)` 写入，ASC 收到输入时遍历所有 Spec 匹配该标签。这样"输入 → 能力"的映射完全数据驱动。

---

## 1. 前置：`IDFCore.Build.cs` 补 `EnhancedInput`

📁 **修改** `Plugins/GameFeatures/IDFCore/Source/IDFCore/IDFCore.Build.cs`

`UIDFInputConfig` 要引用 `UInputMappingContext` / `UInputAction`，所以依赖里必须有 `EnhancedInput`：

```csharp
PublicDependencyModuleNames.AddRange(
    new string[]
    {
        "Core", "CoreUObject", "Engine", "InputCore",
        "GameplayAbilities",
        "GameplayTags",
        "GameplayTasks",
        "EnhancedInput",        // ★ S06 新增：UIDFInputConfig 需要 UInputMappingContext / UInputAction
    }
    );
```

> 为什么放 **Public**：`UIDFInputConfig.h` 在 `Public/` 下，任何 include 它的模块（主模块、各角色插件）都要能看到 `UInputAction` 的声明。放 Private 会导致它们编译时报"未定义类型"。

改完 → **关闭编辑器** → 编译 → 打开编辑器。

> **顺带**：主模块的 `Identity5.Build.cs` 里已经有 `EnhancedInput` 了（向导阶段留下的），不用改。

---

## 2. 本节要建的文件

```
Plugins/GameFeatures/IDFCore/Source/IDFCore/
├── Public/
│   ├── IDFGameplayTags.h                    ← 新增：全部原生标签的 extern 声明
│   ├── AbilitySystem/
│   │   ├── IDFAbilitySet.h                  ← 新增：能力装配数据资产
│   │   ├── IDFInputConfig.h                 ← 新增：输入映射数据资产
│   │   └── IDFTestAbility.h                 ← 新增：最小验证能力（临时，S07 后删或留作夹具）
│   └── (已有) IDFCoreModule.h / AbilitySystem/IDFAbilitySystemComponent.h / IDFHealthSet.h / ...
└── Private/
    ├── IDFGameplayTags.cpp                  ← 新增：全部标签的定义（只能这一处）
    └── AbilitySystem/
        ├── IDFAbilitySet.cpp                ← 新增
        └── IDFTestAbility.cpp               ← 新增
```

> `UIDFInputConfig` 只声明数据（`UInputMappingContext` + 一堆 `UInputAction` 指针），**没有实现逻辑**，所以不需要 `.cpp`——纯数据资产的 `UCLASS` 可以只在头文件里定义。这是 UE 里很常见的做法（记得加 `UCLASS(BlueprintType, Const)`）。

---

## 3. `IDFGameplayTags.h` / `.cpp`（原生标签）

### 3.1 命名约定与本节范围

**命名约定**（来自设计文档第 2 轮 5.1 节，这里落实成代码风格）：

| 前缀 | 用途 | 本节是否建 |
|---|---|---|
| `Input.*` | 输入动作 → 能力 | ✅ 本节需要 |
| `State.*` | 角色/对局状态 | ✅ 建核心几个，其余按需加 |
| `Ability.*` | 能力标识 | ✅ 建几个代表性的 |
| `Interact.*` | 交互选项标签 | ⏳ S10~S13 用到时再加 |
| `Event.*` | GameplayMessage 事件 | ⏳ S08 起用到时再加 |
| `Cooldown.*` / `Block.*` | 冷却与阻断 | ⏳ 等有 CD 的能力时再加 |

> **重要**：**不要一次把设计文档里那几百个标签全写完**。标签要在真正用到时才加，否则会有大量"定义了但没人引用"的死标签，而且拼写错误要到很晚才暴露。**每加一个能力/交互点时顺手加它需要的标签**，这是可持续的节奏。

### 3.2 头文件

```cpp
// IDFGameplayTags.h
#pragma once

#include "NativeGameplayTags.h"

/**
 * 项目原生 GameplayTag 的集中声明处。
 *
 * 使用规则（务必遵守，否则编译失败或产生重复符号）：
 *   - 本文件用 UE_DECLARE_GAMEPLAY_TAG_EXTERN 声明；
 *   - 对应的 UE_DEFINE_GAMEPLAY_TAG 全部写在 IDFGameplayTags.cpp 里，且【只能有一处】；
 *   - 严禁把 UE_DEFINE_GAMEPLAY_TAG 写进头文件（宏内有 static_assert 拦截）。
 *
 * 命名约定：参考 design_analysis.md 第 2 轮 5.1 节。
 * 标签按"用到时才加"的节奏增长，不要一次全写完。
 */
namespace IDFTags
{
    // ═══════════════════════════════════════════════════
    // 输入（Input.*）—— ASC 按这些标签把输入转发给对应能力
    // ═══════════════════════════════════════════════════
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Interact);
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_InteractHold);
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Sprint);
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Crouch);
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Skill1);
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Skill2);
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_UseItem);

    // ═══════════════════════════════════════════════════
    // 能力标识（Ability.*）
    // ═══════════════════════════════════════════════════
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Test);

    // ═══════════════════════════════════════════════════
    // 角色状态（State.*）
    // ═══════════════════════════════════════════════════
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Ready);

    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Survivor_Healthy);
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Survivor_Injured);
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Survivor_Downed);
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Survivor_Carried);
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Survivor_OnChair_Phase1);
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Survivor_OnChair_Phase2);
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Survivor_Escaped);
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Survivor_Eliminated);

    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Hunter_Chasing);
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Hunter_Attacking);
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Hunter_WipingBlade);
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Hunter_Stunned);
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Hunter_Carrying);

    // ═══════════════════════════════════════════════════
    // 属性集用的 SetByCaller 数据标签（Data.*）
    //    S05 留下的 TODO：PostGameplayEffectExecute 里读取伤害/治疗量的正确来源。
    //    GE 里用 SetByCaller(Data.Damage) 传值，代码用这个标签取。
    // ═══════════════════════════════════════════════════
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage);
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Healing);

    // ═══════════════════════════════════════════════════
    // 冷却阻断（Cooldown.*）
    // ═══════════════════════════════════════════════════
    IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Test);
}
```

⚠️ **三个注意点**：

1. **`IDFCORE_API` 放在 `UE_DECLARE_GAMEPLAY_TAG_EXTERN` 前面**——宏展开成 `extern FNativeGameplayTag TagName;`，所以 `IDFCORE_API UE_DECLARE_...` 会变成 `IDFCORE_API extern FNativeGameplayTag ...`。**这个顺序是合法的**（`__declspec(dllimport)` 可以在 `extern` 前后），引擎源码里也是这么写的（例如 `IoStore.h:20` 的 `CORE_API DECLARE_LOG_CATEGORY_EXTERN(...)`）。
2. **全部放在 `namespace IDFTags` 里**——避免污染全局命名空间，调用处写 `IDFTags::Input_Interact`，一眼知道来源。
3. **标签名用下划线而不是点**——C++ 标识符不能含 `.`，所以 `Input.Interact` 的变量名是 `Input_Interact`，而字符串参数才是 `"Input.Interact"`。

### 3.3 `.cpp` 文件

```cpp
// IDFGameplayTags.cpp
//
// 【唯一】的标签定义处。
// 每一个 UE_DEFINE_GAMEPLAY_TAG 只能出现一次，否则链接报 duplicate symbol。
// 这里的顺序与头文件保持一致，便于对照检查"有没有漏定义"。

#include "IDFGameplayTags.h"

namespace IDFTags
{
    // ── 输入 ──
    UE_DEFINE_GAMEPLAY_TAG(Input_Interact,     "Input.Interact");
    UE_DEFINE_GAMEPLAY_TAG(Input_InteractHold, "Input.InteractHold");
    UE_DEFINE_GAMEPLAY_TAG(Input_Sprint,       "Input.Sprint");
    UE_DEFINE_GAMEPLAY_TAG(Input_Crouch,       "Input.Crouch");
    UE_DEFINE_GAMEPLAY_TAG(Input_Skill1,       "Input.Skill1");
    UE_DEFINE_GAMEPLAY_TAG(Input_Skill2,       "Input.Skill2");
    UE_DEFINE_GAMEPLAY_TAG(Input_UseItem,      "Input.UseItem");

    // ── 能力标识 ──
    UE_DEFINE_GAMEPLAY_TAG(Ability_Test,       "Ability.Test");

    // ── 角色状态 ──
    UE_DEFINE_GAMEPLAY_TAG(State_Ready,                 "State.Ready");

    UE_DEFINE_GAMEPLAY_TAG(State_Survivor_Healthy,      "State.Survivor.Healthy");
    UE_DEFINE_GAMEPLAY_TAG(State_Survivor_Injured,      "State.Survivor.Injured");
    UE_DEFINE_GAMEPLAY_TAG(State_Survivor_Downed,       "State.Survivor.Downed");
    UE_DEFINE_GAMEPLAY_TAG(State_Survivor_Carried,      "State.Survivor.Carried");
    UE_DEFINE_GAMEPLAY_TAG(State_Survivor_OnChair_Phase1, "State.Survivor.OnChair.Phase1");
    UE_DEFINE_GAMEPLAY_TAG(State_Survivor_OnChair_Phase2, "State.Survivor.OnChair.Phase2");
    UE_DEFINE_GAMEPLAY_TAG(State_Survivor_Escaped,      "State.Survivor.Escaped");
    UE_DEFINE_GAMEPLAY_TAG(State_Survivor_Eliminated,   "State.Survivor.Eliminated");

    UE_DEFINE_GAMEPLAY_TAG(State_Hunter_Chasing,     "State.Hunter.Chasing");
    UE_DEFINE_GAMEPLAY_TAG(State_Hunter_Attacking,   "State.Hunter.Attacking");
    UE_DEFINE_GAMEPLAY_TAG(State_Hunter_WipingBlade, "State.Hunter.WipingBlade");
    UE_DEFINE_GAMEPLAY_TAG(State_Hunter_Stunned,     "State.Hunter.Stunned");
    UE_DEFINE_GAMEPLAY_TAG(State_Hunter_Carrying,    "State.Hunter.Carrying");

    // ── SetByCaller 数据标签 ──
    UE_DEFINE_GAMEPLAY_TAG(Data_Damage,        "Data.Damage");
    UE_DEFINE_GAMEPLAY_TAG(Data_Healing,       "Data.Healing");

    // ── 冷却 ──
    UE_DEFINE_GAMEPLAY_TAG(Cooldown_Test,      "Cooldown.Test");
}
```

**✅ 自测**：
1. 编译通过（若报 `duplicate symbol TagName` → 你在别处也定义了一次）；
2. PIE 后在控制台执行：
   ```
   GameplayTags.List
   ```
   或打开 **Window → Gameplay Tags** 面板（若可用），能搜到 `Input.Interact`、`State.Ready` 等；
3. 更直接：在 `AIDFGasTestActor::BeginPlay` 里临时加一行打印：
   ```cpp
   UE_LOG(LogIDFAbility, Log, TEXT("Input.Interact 是否有效 = %s"), IDFTags::Input_Interact.IsValid() ? TEXT("是") : TEXT("否"));
   ```
   期望输出 `是`。如果输出 `否`，说明标签没注册成功（检查 `.cpp` 是否真的被编译进去）。

---

## 4. `UIDFAbilitySet`（能力装配数据资产）

### 4.1 头文件

```cpp
// IDFAbilitySet.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "IDFAbilitySet.generated.h"

class UGameplayAbility;
class UGameplayEffect;
class UAttributeSet;
class UAbilitySystemComponent;

/** 一条"授予这个能力"的声明 */
USTRUCT(BlueprintType)
struct IDFCORE_API FIDFAbilitySet_Ability
{
    GENERATED_BODY()

    /** 要授予的能力类 */
    UPROPERTY(EditDefaultsOnly, Category = "IDF|Ability")
    TSubclassOf<UGameplayAbility> Ability;

    /** 能力等级（影响 GE 的等级与数值缩放） */
    UPROPERTY(EditDefaultsOnly, Category = "IDF|Ability")
    int32 Level = 1;

    /**
     * 输入标签：非空表示"这个能力由哪个输入触发"。
     * 授予时会写入 Spec 的 GetDynamicSpecSourceTags()，ASC 按标签匹配输入事件。
     * 留空表示这是被动/条件触发的能力（不由玩家输入直接激活）。
     */
    UPROPERTY(EditDefaultsOnly, Category = "IDF|Ability", meta = (Categories = "Input"))
    FGameplayTag InputTag;
};

/** 一条"施加这个效果"的声明（初始化属性、常驻状态等） */
USTRUCT(BlueprintType)
struct IDFCORE_API FIDFAbilitySet_GameplayEffect
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, Category = "IDF|Effect")
    TSubclassOf<UGameplayEffect> Effect;

    UPROPERTY(EditDefaultsOnly, Category = "IDF|Effect")
    float Level = 1.f;
};

/** 一条"添加这个属性集"的声明 */
USTRUCT(BlueprintType)
struct IDFCORE_API FIDFAbilitySet_AttributeSet
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, Category = "IDF|Attribute")
    TSubclassOf<UAttributeSet> AttributeSet;
};

/**
 * 角色能力装配表。
 *
 * 用法（S07 起）：AIDFCharacterBase 上放一个 UIDFAbilitySet* 默认资产，
 * 在 ASC 初始化完成后调用 GiveToAbilitySystem()。
 *
 * 幂等性：本函数可被重复调用（服务器与客户端都会走一遍初始化），
 * 内部的重复授予不会导致能力列表里出现多份——由 ASC 的 Spec 去重与 WithAbilitySet 标记保证。
 */
UCLASS(BlueprintType)
class IDFCORE_API UIDFAbilitySet : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    /**
     * 把本资产声明的能力/效果/属性集一次性授予目标 ASC。
     *
     * @param ASC          目标能力系统组件（必须已完成 InitAbilityActorInfo）
     * @param OutHandles   可选：返回本次授予的能力 Handle，供将来移除（换角色/重生）用
     */
    void GiveToAbilitySystem(UAbilitySystemComponent* ASC,
                             TArray<FGameplayAbilitySpecHandle>* OutHandles = nullptr) const;

    UPROPERTY(EditDefaultsOnly, Category = "IDF|Abilities")
    TArray<FIDFAbilitySet_Ability> GrantedAbilities;

    UPROPERTY(EditDefaultsOnly, Category = "IDF|Effects")
    TArray<FIDFAbilitySet_GameplayEffect> GrantedEffects;

    UPROPERTY(EditDefaultsOnly, Category = "IDF|Attributes")
    TArray<FIDFAbilitySet_AttributeSet> GrantedAttributeSets;
};
```

### 4.2 `.cpp` 实现要点

**已核实的签名**（都在 `AbilitySystemComponent.h`）：

```cpp
FGameplayAbilitySpecHandle GiveAbility(const FGameplayAbilitySpec& AbilitySpec);                     // :949
FActiveGameplayEffectHandle ApplyGameplayEffectToSelf(const UGameplayEffect* GameplayEffect,
        float Level, const FGameplayEffectContextHandle& EffectContext,
        FPredictionKey PredictionKey = FPredictionKey());                                           // :773
FGameplayEffectContextHandle MakeEffectContext() const;                                             // :376
FGameplayAbilitySpec(TSubclassOf<UGameplayAbility> InAbilityClass, int32 InLevel,
        int32 InInputID, UObject* InSourceObject);                                                  // GameplayAbilitySpec.h:185
void AddSpawnedAttribute(UAttributeSet* Attribute);                                                 // :218
```

✍️ **你要写的逻辑（顺序很重要）**：

```cpp
void UIDFAbilitySet::GiveToAbilitySystem(UAbilitySystemComponent* ASC,
                                        TArray<FGameplayAbilitySpecHandle>* OutHandles) const
{
    if (!IsValid(ASC))
    {
        UE_LOG(LogIDFAbility, Warning, TEXT("UIDFAbilitySet::GiveToAbilitySystem 收到空的 ASC，已跳过"));
        return;
    }

    // ── 第 1 步：添加属性集（必须在应用 Effect 之前！）──
    //   原因：GE 修改属性时要能"找到"该属性所在的 AttributeSet，
    //   属性集不存在时 GE 会静默失败（不报错，但属性不变）。
    for (const FIDFAbilitySet_AttributeSet& SetToGrant : GrantedAttributeSets)
    {
        if (!SetToGrant.AttributeSet) { continue; }

        // 用 AddSet<T>() 的泛型版本：GetOrCreateAttributeSubobject 会检查是否已存在，
        // 所以重复调用不会创建第二个实例。
        // 注意 AddSpawnedAttribute 需要【裸指针】，这里用 NewObject 出来的对象直接传。
        UAttributeSet* NewSet = NewObject<UAttributeSet>(ASC, SetToGrant.AttributeSet);
        ASC->AddSpawnedAttribute(NewSet);
    }

    // ── 第 2 步：应用初始 GameplayEffect ──
    //   典型用途：GE_Init_BaseAttributes（把 MaxHealth 之类初始化成配置值）。
    //   注意 MakeEffectContext() → MakeOutgoingSpec() 的固定套路：
    //   Context 描述"这个效果从哪来"，Spec 是"这次执行的实例"。
    for (const FIDFAbilitySet_GameplayEffect& EffectToGrant : GrantedEffects)
    {
        if (!EffectToGrant.Effect) { continue; }

        FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
        Context.AddSourceObject(ASC->GetAvatarActor());   // 让 GE 里能追溯到施加者

        const FGameplayEffectSpecHandle SpecHandle =
            ASC->MakeOutgoingSpec(EffectToGrant.Effect, EffectToGrant.Level, Context);

        if (SpecHandle.IsValid())
        {
            ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
        }
    }

    // ── 第 3 步：授予能力（放最后，因为能力的激活条件可能依赖属性/标签已就绪）──
    for (const FIDFAbilitySet_Ability& AbilityToGrant : GrantedAbilities)
    {
        if (!AbilityToGrant.Ability) { continue; }

        // 输入标签走 DynamicSpecSourceTags（老的 DynamicAbilityTags 已在 5.5 废弃）
        FGameplayAbilitySpec Spec(AbilityToGrant.Ability, AbilityToGrant.Level, INDEX_NONE, ASC->GetAvatarActor());

        if (AbilityToGrant.InputTag.IsValid())
        {
            Spec.GetDynamicSpecSourceTags().AddTag(AbilityToGrant.InputTag);
        }

        const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);

        if (OutHandles)
        {
            OutHandles->Add(Handle);
        }

        UE_LOG(LogIDFAbility, Verbose, TEXT("已授予能力 %s（等级 %d，输入标签 %s）"),
            *GetNameSafe(AbilityToGrant.Ability.Get()),
            AbilityToGrant.Level,
            AbilityToGrant.InputTag.IsValid() ? *AbilityToGrant.InputTag.ToString() : TEXT("<无>"));
    }
}
```

⚠️ **五个必须理解的判断**：

| 点 | 说明 |
|---|---|
| **`IsValid(ASC)` 检查** | `GiveToAbilitySystem` 会在双端初始化时被调用，参数可能为空（例如 PlayerState 还没到）。**不要 `check()`**，这是可恢复的情况 |
| **属性集必须最先加** | GE 改属性时要能找到属性集，否则**静默失败**（不报错、属性不变——极难排查） |
| **`NewObject` 而不是 `CreateDefaultSubobject`** | 数据资产是在运行时装配的，`CreateDefaultSubobject` 只能在构造函数里用。这里用 `NewObject<UAttributeSet>(ASC, 类)` 把 ASC 作为 Outer，语义上等价 |
| **`MakeEffectContext` 两步走** | `Context` 是"效果从哪来"，`Spec` 是"这次执行的实例"。少了 `AddSourceObject` 的话，`PostGameplayEffectExecute` 里的 `GetOriginalInstigatorAbilitySystemComponent()` 会是空 |
| **`GiveAbility` 放最后** | 能力的 `CanActivateAbility` 可能检查标签/属性，先让属性与效果就位更稳 |

> **关于 `AddSpawnedAttribute` vs `AddAttributeSetSubobject(Subobject)`**：
> 后者是 `template<class T> const T* AddAttributeSetSubobject(T* Subobject)`，内部就是 `AddSpawnedAttribute(Subobject)` 再返回。
> 两者等价——`AddSpawnedAttribute` 直接、不需要模板推导，在"运行时用 `NewObject` 造出来的对象"场景下更直接。我在 S05 里用 `AddAttributeSetSubobject` 是因为那里传的是 `CreateDefaultSubobject` 的结果，这里用 `AddSpawnedAttribute` 更贴合。

### 4.3 `UPrimaryDataAsset` 与 `UDataAsset` 的区别（顺带说清）

| 类型 | 特点 | 适用 |
|---|---|---|
| `UDataAsset` | 最简纯粹的数据容器 | 纯配置数据 |
| **`UPrimaryDataAsset`** | 多一个 `AssetManager` 可识别的 `PrimaryAssetId` | **需要被资产管理器扫描、按需加载、参与打包分块**——角色 AbilitySet 属于这类 |

选 `UPrimaryDataAsset` 是有意的：S20 做"选角色时才加载对应角色插件"时，资产管理器需要能按 ID 定位这些资产。

**因此资产的命名要有规律**（S20 的扫描规则会依赖它）：

```
DA_Character_Doctor_AbilitySet
DA_Character_Airman_AbilitySet
DA_Input_Survivor
DA_Input_Hunter
```

---

## 5. `UIDFInputConfig`（输入映射数据资产）

### 5.1 头文件（只有头文件，无 `.cpp`）

```cpp
// IDFInputConfig.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "IDFInputConfig.generated.h"

class UInputMappingContext;
class UInputAction;

/**
 * 一组"输入动作 → 用途"的映射声明。
 *
 * 与 UIDFAbilitySet 的分工：
 *   - 本资产声明"玩家按键对应哪个 EnhancedInput 动作"；
 *   - UIDFAbilitySet 声明"某个能力由哪个输入【标签】触发"。
 *   桥接方式是：每个 AbilityInputAction 在这里带上 InputTag，
 *   输入触发时把该标签传给 ASC，由 ASC 匹配到对应能力。
 */
UCLASS(BlueprintType, Const)
class IDFCORE_API UIDFInputConfig : public UDataAsset
{
    GENERATED_BODY()

public:
    /** 该配置对应的输入映射上下文（一个角色可以切换多个） */
    UPROPERTY(EditDefaultsOnly, Category = "IDF|Input")
    TObjectPtr<UInputMappingContext> DefaultMappingContext;

    // ── 原生输入：不走 GAS，直接由角色处理 ──
    //   移动/视角必须在角色上直接处理，因为它们每个帧都要读轴值，
    //   走 GAS 会引入不必要的开销与延迟。这也是 Lyra 的做法。

    UPROPERTY(EditDefaultsOnly, Category = "IDF|Input|Native")
    TObjectPtr<UInputAction> MoveAction;

    UPROPERTY(EditDefaultsOnly, Category = "IDF|Input|Native")
    TObjectPtr<UInputAction> LookAction;

    /** 本作没有跳跃；这个动作绑定到"翻越"（S14 起用） */
    UPROPERTY(EditDefaultsOnly, Category = "IDF|Input|Native")
    TObjectPtr<UInputAction> VaultAction;

    UPROPERTY(EditDefaultsOnly, Category = "IDF|Input|Native")
    TObjectPtr<UInputAction> CrouchAction;

    UPROPERTY(EditDefaultsOnly, Category = "IDF|Input|Native")
    TObjectPtr<UInputAction> SprintAction;

    // ── 能力输入：走 GAS ──
    //   这些动作触发时，把 InputTag 交给 ASC，由 ASC 找匹配的能力激活。

    /** 一个输入动作 + 它对应的能力输入标签 */
    UPROPERTY(EditDefaultsOnly, Category = "IDF|Input|Ability")
    TArray<TObjectPtr<UInputAction>> AbilityInputActions;

    /**
     * 与 AbilityInputActions 一一对应的输入标签。
     * ⚠️ 用两个平行数组是 UE 里常见的省事做法，但容易错位。
     *    如果将来发现"按 A 触发了 B 的技能"，先检查这两个数组的长度是否一致。
     */
    UPROPERTY(EditDefaultsOnly, Category = "IDF|Input|Ability", meta = (Categories = "Input"))
    TArray<FGameplayTag> AbilityInputTags;
};
```

⚠️ **为什么 `UCLASS(BlueprintType, Const)` 里的 `Const`**：UE 对**纯数据资产**（没有可变运行时状态）推荐加 `Const`，它会让该类的所有 `UPROPERTY` 在编辑器中视为只读、并让 `UObject` 实例标记为 const。**本项目的数据资产都加它**（`UIDFInputConfig`、`UIDFAbilitySet` 其实也可以加——但 `UIDFAbilitySet` 我在上面没加，你如果想统一，给两个都加上）。

### 5.2 平行数组的隐患（值得现在就避免）

上面 `AbilityInputActions` 与 `AbilityInputTags` 两个平行数组是**最容易出错的数据结构**：长度不一致、顺序错位，都不会有任何提示。

**更稳的替代**（推荐你直接这么做）：定义一个 USTRUCT 把两者捆在一起：

```cpp
USTRUCT(BlueprintType)
struct IDFCORE_API FIDFInputAction
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, Category = "IDF|Input")
    TObjectPtr<UInputAction> InputAction;

    UPROPERTY(EditDefaultsOnly, Category = "IDF|Input", meta = (Categories = "Input"))
    FGameplayTag InputTag;
};

// 然后在 UIDFInputConfig 里只用一个数组：
UPROPERTY(EditDefaultsOnly, Category = "IDF|Input|Ability")
TArray<FIDFInputAction> AbilityInputActions;
```

**这样结构上就不可能错位**——这就是"用类型系统消除一类 bug"的思路。平行数组在 UE 里之所以常见，很多是历史遗留；新代码没有理由重复它。

---

## 6. `UIDFTestAbility`（最小验证能力）

S06 还不需要真正的角色技能，但**必须有一个能力来验证整条链路**（输入标签 → 授予 → 激活 → 执行）。

```cpp
// IDFTestAbility.h
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "IDFTestAbility.generated.h"

/**
 * 最小验证能力：激活时在屏幕与日志打一条消息，然后立刻结束。
 *
 * 用途：S06 验证"AbilitySet 授予 → 输入标签匹配 → ASC 激活"整条链路。
 * 生命周期：S07 起可以被真正的角色能力替代；保留它作为"链路自检"夹具也无妨。
 */
UCLASS()
class IDFCORE_API UIDFTestAbility : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UIDFTestAbility();
};
```

```cpp
// IDFTestAbility.cpp
#include "AbilitySystem/IDFTestAbility.h"
#include "AbilitySystemComponent.h"
#include "Logging/IDFCoreLog.h"
#include "IDFGameplayTags.h"

UIDFTestAbility::UIDFTestAbility()
{
    // 【必须】声明"我响应哪个输入标签"。
    // ASC 在 AbilityInputTagPressed(Input.Skill1) 时会遍历所有 Spec，
    // 匹配 GetDynamicSpecSourceTags() 或本能力的 AbilityTags。
    //
    // 注意：AbilitySet 授予时也会写一次到 Spec 的 DynamicSpecSourceTags。
    //       两处都写是【有意冗余】——本类自己能独立工作（不依赖 AbilitySet 配置正确），
    //       便于排查"到底是资产配错了还是能力写错了"。
    FGameplayTagContainer Tags;
    Tags.AddTag(IDFTags::Ability_Test);
    SetAssetTags(Tags);

    // 瞬发能力：不实例化、不复制，激活即结束
    InstancingPolicy = EGameplayAbilityInstancingPolicy::NonInstanced;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

// 若用 NonInstanced，激活逻辑要写在 ActivateAbility 里；
// 若将来需要状态（读条），改成 InstancedPerActor 并重写 ActivateAbility/EndAbility。
```

⚠️ **关于 `NonInstanced` + `ActivateAbility` 的坑**：`NonInstanced`（即 `NonInstanced` 策略）下**不能使用 `AbilityTask`**，也不能保存成员状态。S06 的测试能力只是打日志，可以；但**真正的角色技能（自疗读条等）必须用 `InstancedPerActor`**。

> **怎么激活它？** S06 阶段还没有输入组件（S07 才建 `AIDFCharacterBase`），所以临时用控制台命令激活：
> ```cpp
> // 临时调试命令（放在 Identity5DebugCommands.cpp 或测试 Actor 里）
> // 让 AIDFGasTestActor 暴露一个函数：
> if (UIDFAbilitySystemComponent* ASC = ...)
> {
>     ASC->TryActivateAbilityByTag(IDFTags::Ability_Test);
> }
> ```
> 但 `TryActivateAbilityByTag` 是 S07 才要实现的（设计文档第 2 轮 6.1 节）。**S06 更简单的验证方式**：在 `AIDFGasTestActor::BeginPlay` 里直接遍历授予的 Spec 手动激活第一个：
> ```cpp
> for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
> {
>     ASC->TryActivateAbility(Spec.Handle);
> }
> ```
> 能看到测试能力的日志输出，就说明"AbilitySet → GiveAbility → 激活"链路是通的。

---

## 7. 自测清单（本节 DoD）

| # | 验证项 | 期望结果 |
|---|---|---|
| 1 | 编译通过 | 无 Error。常见错误见下方坑表 |
| 2 | 标签已注册 | 打印 `IDFTags::Input_Interact.IsValid()` → `是`；或控制台 `GameplayTags.List` 里能搜到 |
| 3 | 标签有插件归属 | 编辑器 Gameplay Tags 面板里，这些标签的"来源"显示为 `Identity5`（因为 `UE_PLUGIN_NAME=Identity5`）。**这是预期行为，不是错误** |
| 4 | 资产可创建 | Content Browser 里能新建 `IDFAbilitySet` / `IDFInputConfig` 类型的资产（右键 → Miscellaneous → Data Asset → 选类） |
| 5 | AbilitySet 生效 | 给测试 Actor 挂上 `DA_IDF_AbilitySet_Placeholder` 后，`ASC->PrintDebugInfo()` 里 `能力[0] = IDFTestAbility` |
| 6 | 属性集由 AbilitySet 添加 | 如果 AbilitySet 里配了 `IDFHealthSet`，日志里 `属性集[0] = IDFHealthSet`；若同时用 S05 的 `CreateDefaultSubobject` 方式加，**不应该出现两个 IDFHealthSet**（`AddSpawnedAttribute` 内部按指针去重，但两个不同实例会都进去——检查一下） |
| 7 | 输入标签写入了 Spec | 遍历 `GetActivatableAbilities()`，`Spec.GetDynamicSpecSourceTags()` 里含 `Input.Skill1` |
| 8 | 能力能被激活 | 手动 `TryActivateAbility(Spec.Handle)` 后，日志里有测试能力的输出 |

### 验证项 6 的注意点（容易搞出两个属性集）

如果你在测试 Actor 构造函数里 `CreateDefaultSubobject<UIDFHealthSet>` **同时**在 AbilitySet 里配了 `IDFHealthSet`，会**创建两个不同实例**（一个随 Actor 构造，一个 `NewObject`），`AddSpawnedAttribute` 的 `Find(Attribute) == INDEX_NONE` 是按**指针**去重的，所以两个都会进去。

**应对**：二选一。
- **推荐**：属性集交给 AbilitySet 声明（数据驱动，加角色时不用改代码）；
- 或者保留 `CreateDefaultSubobject`，AbilitySet 的 `GrantedAttributeSets` **留空**。

**S07 建 `AIDFCharacterBase` 时按"推荐"做**——属性集由 AbilitySet 装配，这样"医生比普通求生者多一个属性集"就变成了纯数据差异。

---

## 8. 常见坑汇总

| 现象 | 原因 | 处理 |
|---|---|---|
| 编译错：`UE_DEFINE_GAMEPLAY_TAG can only be used in .cpp files` | 把 `UE_DEFINE_GAMEPLAY_TAG` 写进了头文件（宏内有 `static_assert` 拦截） | 头文件用 `UE_DECLARE_GAMEPLAY_TAG_EXTERN`，定义移到 `.cpp` |
| 链接错：`duplicate symbol Input_Interact` | 在多个 `.cpp` 里定义同一个标签 | 全工程只能有一处 `UE_DEFINE_GAMEPLAY_TAG` |
| 编译错：`UInputAction 未定义` | `IDFCore.Build.cs` 缺 `EnhancedInput` | 加进 `PublicDependencyModuleNames` |
| 运行时报 `Ensure condition failed` 找不到标签 | 标签字符串拼写与声明不一致（如 `Input.Interact` 写成 `Input.Interects`） | 字符串只写一次（在 `.cpp` 里），其他全用常量 |
| **GE 改了属性但值不变，且没有任何报错** | 属性集没加进 ASC（GE 找不到属性所在 Set，静默失败） | 检查 `GiveToAbilitySystem` 里属性集是否在**应用 Effect 之前**添加 |
| 能力授予了但输入触发不了 | `Spec.GetDynamicSpecSourceTags()` 没写入 `InputTag`，或能力自身的 `AbilityTags` 不匹配 | 检查 AbilitySet 里的 `InputTag` 是否填了；`IDF.Debug.GAS 1` 打印看 Spec 的标签 |
| `DA_*` 资产在 Content Browser 里建不出来 | 类没编译成功 / `UCLASS` 漏了 `BlueprintType` | 确认编译；`UPrimaryDataAsset` 子类需要 `BlueprintType` 才能在建资产菜单里出现 |
| 资产内容改了但运行时没变 | 改了资产没保存（数据资产也需要 `Save All`） | `File → Save All` |
| **改资产后行为没变** | PIE 里旧资产被缓存，或你改的是 `Saved/` 下的副本 | 停止 PIE 再改；确认改的是 `Plugins/GameFeatures/IDFCore/Content/` 下的资产 |

---

## 9. 提交

```powershell
cd 'G:\Project\Unreal Projects\Identity5'

git add Plugins/GameFeatures/IDFCore/Source `
        Plugins/GameFeatures/IDFCore/Content `
        Config/DefaultEngine.ini

git status --short      # 确认没有 Binaries/Intermediate 混进来
git commit -m "feat(gas): 原生 GameplayTag 注册 + AbilitySet/InputConfig 数据资产，能力装配改为数据驱动"
```

> 提交前确认 `Content/` 下的新资产真的在里面（`git status` 应该看到 `.uasset` 文件）。`Content/` 里的 `*.uasset` 走 Git LFS，第一次提交时 Git 会调用 LFS。

---

## 10. 简历点与面试问答

💼 **可以这样写**：

> **数据驱动的能力装配**：用 `UPrimaryDataAsset` 实现 `IDFAbilitySet`（聚合能力/初始 GE/属性集三类声明）与 `IDFInputConfig`，把"角色有哪些能力、由哪个输入触发"从 C++ 硬编码改为编辑器内配置；GameplayTag 全部采用 C++ 原生注册（`UE_DECLARE_GAMEPLAY_TAG_EXTERN` + `UE_DEFINE_GAMEPLAY_TAG`），获得编译期拼写检查、零字符串查找，并明确区分头文件声明与单一定义以避免重复符号。

**可能被追问的问题**：

1. **"原生 GameplayTag 和 ini 标签表有什么区别？为什么选原生？"**
   → 原生在静态初始化阶段注册，编译期检查拼写、无字符串查找、不依赖 ini 加载顺序；代价是改标签要重编译（对代码引用的标签来说这反而是好事）。ini 表适合"策划要频繁改、代码不引用"的标签。

2. **"为什么 `UE_DEFINE_GAMEPLAY_TAG` 只能写在 `.cpp` 且只能有一处？"**
   → 它定义一个全局对象；写在头文件会在每个 include 它的翻译单元里各定义一份 → 重复符号。引擎用宏内 `static_assert` 强制拦截，并用 `UE_DECLARE_GAMEPLAY_TAG_EXTERN` 提供声明侧。

3. **"`UIDFAbilitySet` 为什么继承 `UPrimaryDataAsset` 而不是 `UDataAsset`？"**
   → `UPrimaryDataAsset` 提供 `PrimaryAssetId`，能被资产管理器扫描/按需加载/参与打包分块。S20 的"选角色时才加载角色插件资产"依赖这个能力。

4. **"`GiveToAbilitySystem` 里为什么属性集必须放在最前面？"**
   → GE 修改属性时要通过属性所在 Set 定位；Set 不存在时 GE 会**静默失败**（不报错、属性不变）。把顺序固定下来能消除这类极难排查的问题。

5. **"输入为什么分'原生'和'能力'两类？"**
   → 移动/视角是每帧读轴值的连续输入，直接在角色上处理、走 `CharacterMovement` 更高效；技能是离散事件，走 GAS 才能享受预测/冷却/标签阻断等能力。混在一起会既慢又难维护。

---

## 11. 下一步

S06 完成后进 **S07：`AIDFCharacterBase` 的 ASC 双端初始化**。那时：

- `AIDFGasTestActor` 的初始化逻辑会搬进 `AIDFCharacterBase`（本节验证过的链路会被复用）；
- `UIDFAbilitySet` 会挂到角色基类上，用真实角色验证；
- S05 留下的另一个 TODO（`PostGameplayEffectExecute` 用 `Data.Damage` 取 SetByCaller 值）可以在那时一起完成——标签本节已经备好了。
