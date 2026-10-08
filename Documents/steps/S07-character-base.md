# S07 实施步骤：角色基类 `AIDFCharacterBase`（ASC 双端初始化）

> 对应 `implementation_roadmap.md` 的 S07。**目标**：服务器与客户端**都能**正确初始化 ASC，两端都能读到 `State.Ready` 与正确的属性值、授予的能力。
> 本节预计 5~7 小时。**这是整个项目面试考点最密集的一节**——"GAS 为什么在客户端没生效"是 GAS 面试的第一高频问题，而答案就在本节的初始化时序里。

---

## 0. 先纠正设计文档里的两处假设（我核实过引擎源码）

### 0.1 `GameplayMessageSubsystem` **不在引擎里**——它是 Lyra 项目自带的

设计文档第 2 轮 ADR-8 与 3.1 节假设"引擎提供 `UGameplayMessageSubsystem`"。**这个假设是错的**：

```
搜索 "GameplayMessageSubsystem.h" 于整个 Engine 目录 → 无结果
```

它是 **Lyra 样例工程**里自己实现的一个类（放在 Lyra 的 `GameplayMessageRuntime` 插件里），不是引擎能力。

**所以本节有两个选择**：

| 方案 | 做法 | 评价 |
|---|---|---|
| **A（本节采用）** | 本节**先不引入消息系统**，用 `UGameplayMessageSubsystem` 的**替代品**：在 `AIDFCharacterBase` 上放一个 `BlueprintAssignable` 委托 `OnCharacterReady` | ✅ 最省事，够用很久。等真的需要"多方解耦广播"时（S13 破译完成、S18 挂椅事件）再抽出子系统 |
| B | 现在就自己写一个 `UIDFGameplayMessageSubsystem : UGameInstanceSubsystem`（约 100 行：`BroadcastMessage(Tag, Payload)` + `RegisterListener`） | 早点有基础设施，但 S07 的重点是初始化时序，同时做两件事容易两头顾不上 |

**我建议 A**，并在文档末尾给出 B 的接口设计，等 S13 需要时再实现。**不要现在就为了"和设计文档一致"去实现一个你还没理解用处的子系统。**

### 0.2 `InitState.*` 这些 GameplayTag **也不在引擎里**

设计文档第 2 轮 3.2 节引用了 `InitState.Spawned / DataAvailable / DataInitialized / GameplayReady` 这套标签。我核实过：

```
搜索 "InitState.Spawned" / "GameFramework.InitState" / "InitStateOrder 的赋值处"
  → 在 ModularGameplay 插件、Engine 模块、引擎 Config 里都没有定义
```

`UGameFrameworkComponentManager` 的 `RegisterInitState()` 是**给外部调用**的（它只维护一个 `InitStateOrder` 数组），那套标签名是**使用方自己定的约定**（Lyra 里定义），引擎不提供。

**这反而是好事**：**标签归我们定义**。在 `IDFGameplayTags.h` 里加：

```cpp
// 初始化状态标签（对应 UGameFrameworkComponentManager 的状态链）
// 注意：引擎不提供这组标签，是使用方的约定。我们沿用 Lyra 的命名以便对照资料。
IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InitState_Spawned);
IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InitState_DataAvailable);
IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InitState_DataInitialized);
IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InitState_GameplayReady);
```

```cpp
// IDFGameplayTags.cpp
UE_DEFINE_GAMEPLAY_TAG(InitState_Spawned,          "InitState.Spawned");
UE_DEFINE_GAMEPLAY_TAG(InitState_DataAvailable,    "InitState.DataAvailable");
UE_DEFINE_GAMEPLAY_TAG(InitState_DataInitialized,  "InitState.DataInitialized");
UE_DEFINE_GAMEPLAY_TAG(InitState_GameplayReady,    "InitState.GameplayReady");
```

⚠️ **注册状态顺序也需要自己做**。`UGameFrameworkComponentManager` 的 `InitStateOrder` 默认是空数组，`IsInitStateAfterOrEqual()` 依赖它的顺序。所以需要在某处（模块启动或 GameInstance 初始化时）按顺序调用：

```cpp
UGameFrameworkComponentManager* Manager = ...;
Manager->RegisterInitState(IDFTags::InitState_Spawned,       /*bAddBefore=*/false, FGameplayTag());
Manager->RegisterInitState(IDFTags::InitState_DataAvailable, false, IDFTags::InitState_Spawned);
Manager->RegisterInitState(IDFTags::InitState_DataInitialized, false, IDFTags::InitState_DataAvailable);
Manager->RegisterInitState(IDFTags::InitState_GameplayReady, false, IDFTags::InitState_DataInitialized);
```

> **本节的取舍**：`IGameFrameworkInitStateInterface` 的价值在于"**让角色插件能在角色生成后异步注入组件/能力，而不改角色基类代码**"——这是 S20 才真正需要的能力。
> **所以本节建议：先把 ASC 双端初始化做对（这是 GAS 的硬需求），`InitState` 那套机制留一个最小骨架（标签 + `CheckDefaultInitialization` 的空实现）**，等 S20 做角色插件时再真正用起来。
> **不要为了"完整实现 InitState"而拖慢 GAS 初始化这条主线**——后者一旦错了，后面每一节都会受牵连。

---

## 1. 本节的核心：ASC 的"双端初始化时序"

### 1.1 为什么需要两套初始化路径

同一份角色类代码，在**服务器**和**客户端**上被"唤醒"的时机完全不同：

| 端 | 角色是怎么出现的 | 哪个回调可靠 |
|---|---|---|
| **服务器** | `GameMode::SpawnDefaultPawnFor()` 直接 Spawn 出来，立刻 `Possess` | **`PossessedBy(AController*)`** ✅ |
| **客户端** | 服务器复制 `APawn` 与 `APlayerController` 过来，客户端**不会**收到 `PossessedBy` 意义上的"本地 Possess 流程"（实际上 `ACharacter` 的 `PossessedBy` **在客户端不会被调用**） | **`OnRep_PlayerState()`** 或 `OnRep_Controller()` ✅ |

**这就是"客户端 ASC 是空"的根因**：新手只在 `PossessedBy` 里写初始化 → 服务器正常、客户端全废。

### 1.2 两条路径必须收敛到同一个函数

```cpp
void AIDFCharacterBase::PossessedBy(AController* NewController)   // 服务器路径
{
    Super::PossessedBy(NewController);
    InitializeAbilitySystem();     // ← 同一个函数
}

void AIDFCharacterBase::OnRep_PlayerState()                        // 客户端路径
{
    Super::OnRep_PlayerState();
    InitializeAbilitySystem();     // ← 同一个函数
}
```

**并且 `InitializeAbilitySystem()` 必须幂等**——`OnRep_PlayerState` 在**服务器上也会被调用**（PlayerState 复制给自己时），而且客户端可能触发多次。所以函数开头要有守卫：

```cpp
if (bAbilitySystemInitialized) { return; }
```

### 1.3 初始化内部的顺序不能动（这是本节最核心的知识点）

```
① InitAbilityActorInfo(OwnerActor, AvatarActor)   ← 必须先做
② 添加 AttributeSet                                ← GE 改属性时要靠它定位属性
③ 应用 AbilitySet 里的初始 GameplayEffect           ← 初始化 MaxHealth 等
④ 授予能力（AbilitySet）
⑤ 加 State.Ready 标签
⑥ 广播"角色就绪"
```

| 顺序 | 如果颠倒会怎样 |
|---|---|
| ② 在 ① 之前 | 属性集找不到 Owner，`PostGameplayEffectExecute` 里 `AbilityActorInfo` 无效 → 结算时读不到 Instigator，甚至崩溃 |
| ③ 在 ② 之前 | GE 找不到属性所在 Set → **静默失败**（不报错、属性不变，最难查） |
| ④ 在 ③ 之前 | 能力的 `CanActivateAbility` 若检查属性/标签会误判 |
| ⑤ 在 ④ 之前 | 依赖 `State.Ready` 做门控的能力会被挡住 |

> **`AbilitySystemComponent` 到底挂谁**：设计文档第 2 轮 9.1 决定"挂 Character"（本项目无重生需求）。所以 `InitAbilityActorInfo(this, this)`。
> 如果将来要支持"死亡后重生且保留能力"，就得改成挂 `PlayerState`，并把 `InitAbilityActorInfo(PlayerState, Character)` —— **接口层已经用 `IAbilitySystemInterface` 抽象好了，改起来只动这一个函数**，这也是当初选接口的原因。

---

## 2. 要建/要改的文件

```
Plugins/GameFeatures/IDFCore/Source/IDFCore/
├── Public/
│   ├── IDFGameplayTags.h                       ← 改：加 4 个 InitState 标签
│   ├── Characters/
│   │   ├── IDFCharacterBase.h                  ← 新增：角色基类
│   │   ├── IDFSurvivorCharacter.h              ← 新增：求生者基类（抽象）
│   │   └── IDFHunterCharacter.h                ← 新增：监管者基类（抽象）
│   └── (已有) AbilitySystem/... Debug/... Logging/...
└── Private/
    ├── IDFGameplayTags.cpp                     ← 改：加 4 个标签定义
    └── Characters/
        ├── IDFCharacterBase.cpp                ← 新增
        ├── IDFSurvivorCharacter.cpp            ← 新增
        └── IDFHunterCharacter.cpp              ← 新增
```

**`IDFCore.Build.cs` 需要补的依赖**：

```csharp
PublicDependencyModuleNames.AddRange(new string[]
{
    "Core", "CoreUObject", "Engine", "InputCore",
    "GameplayAbilities", "GameplayTags", "GameplayTasks",
    "EnhancedInput",        // S06 已加；本节 InputConfig 要用
    "ModularGameplay",      // ★ 本节新增：IGameFrameworkInitStateInterface
    "DeveloperSettings",    // 可选：若要用 UDeveloperSettings（S04 设计里的调试设置）
});
```

---

## 3. `AIDFCharacterBase` 头文件

```cpp
// IDFCharacterBase.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Components/GameFrameworkInitStateInterface.h"   // ModularGameplay 插件
#include "IDFCharacterBase.generated.h"

class UIDFAbilitySystemComponent;
class UIDFAbilitySet;
class UIDFInputConfig;
class UIDFInteractionComponent;
class UIDFNetClockComponent;
class UAttributeSet;

/** 阵营 */
UENUM(BlueprintType)
enum class EIDFTeam : uint8
{
    None     UMETA(DisplayName = "无"),
    Survivor UMETA(DisplayName = "求生者"),
    Hunter   UMETA(DisplayName = "监管者")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FIDFOnCharacterReady, AIDFCharacterBase*, Character);

/**
 * 所有角色的基类。
 *
 * 职责（且仅此）：
 *   1) 持有并初始化 ASC 与 AttributeSet（双端都要）
 *   2) 按 AbilitySet 装配能力与初始效果
 *   3) 提供阵营/生命状态的复制与查询
 *   4) 通过 InitState 机制向 GameFeature 插件暴露"可注入"的时机（S20 用）
 *
 * 不负责：移动手感（S14）、输入具体绑定（S14/S19）、伤害结算（在 AttributeSet 里）
 */
UCLASS(Abstract)
class IDFCORE_API AIDFCharacterBase : public ACharacter,
                                      public IAbilitySystemInterface,
                                      public IGameFrameworkInitStateInterface
{
    GENERATED_BODY()

public:
    AIDFCharacterBase();

    //~ IAbilitySystemInterface
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    //~ IGameFrameworkInitStateInterface
    virtual FName GetFeatureName() const override;
    virtual void CheckDefaultInitialization() override;

    UFUNCTION(BlueprintPure, Category = "IDF|Team")
    EIDFTeam GetTeam() const { return Team; }

    UFUNCTION(BlueprintPure, Category = "IDF|Team")
    bool IsSurvivor() const { return Team == EIDFTeam::Survivor; }

    UFUNCTION(BlueprintPure, Category = "IDF|Team")
    bool IsHunter() const { return Team == EIDFTeam::Hunter; }

    /** 是否为"就绪"状态（ASC 与能力装配完成）。UI 与 AI 应以此为门控 */
    UFUNCTION(BlueprintPure, Category = "IDF|Character")
    bool IsCharacterReady() const { return bAbilitySystemInitialized; }

    /** 角色就绪时广播（本地与远端都会触发一次）。UI/表现层订阅它，而不是在 BeginPlay 抢跑 */
    UPROPERTY(BlueprintAssignable, Category = "IDF|Character")
    FIDFOnCharacterReady OnCharacterReady;

protected:
    virtual void BeginPlay() override;

    //~ 双端初始化入口
    virtual void PossessedBy(AController* NewController) override;   // 服务器
    virtual void OnRep_PlayerState() override;                       // 客户端

    /** 双端共用的初始化实现。必须幂等 */
    virtual void InitializeAbilitySystem();

    /** 装配 AbilitySet（属性集/GE/能力）。单独拆出来便于子类覆写 */
    virtual void ApplyAbilitySet();

    //~ 组件
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "IDF|Components")
    TObjectPtr<UIDFAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "IDF|Components")
    TObjectPtr<UIDFInteractionComponent> InteractionComponent;   // S11 才有实现，先占位可注释

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "IDF|Components")
    TObjectPtr<UIDFNetClockComponent> NetClockComponent;         // S13 才有实现，先占位可注释

    //~ 装配配置（由蓝图子类/角色插件指定）
    UPROPERTY(EditDefaultsOnly, Category = "IDF|Ability")
    TObjectPtr<UIDFAbilitySet> DefaultAbilitySet;

    UPROPERTY(EditDefaultsOnly, Category = "IDF|Input")
    TObjectPtr<UIDFInputConfig> InputConfig;

    //~ 复制状态
    UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadOnly, Category = "IDF|Team")
    EIDFTeam Team = EIDFTeam::None;

    UPROPERTY(ReplicatedUsing = OnRep_LifeStateTag, BlueprintReadOnly, Category = "IDF|State")
    FGameplayTag LifeStateTag;

    UFUNCTION()
    void OnRep_LifeStateTag();

    /** 幂等守卫 */
    bool bAbilitySystemInitialized = false;

    /** 移动参数默认值（S14 会调，这里先给出来） */
    UPROPERTY(EditDefaultsOnly, Category = "IDF|Movement")
    float WalkSpeed = 400.f;

    UPROPERTY(EditDefaultsOnly, Category = "IDF|Movement")
    float SprintSpeed = 550.f;
};
```

### 3.1 两个必须注意的头文件细节

| 项 | 说明 |
|---|---|
| `#include "Components/GameFrameworkInitStateInterface.h"` | 来自 **ModularGameplay 插件**。路径是 `Plugins/Runtime/ModularGameplay/Source/ModularGameplay/Public/Components/GameFrameworkInitStateInterface.h`（我核实过，注意它在 `Components/` 子目录下，不是在插件的 Public 根） |
| `#include "AbilitySystemInterface.h"` | 来自 **GameplayAbilities 插件**（`Plugins/Runtime/GameplayAbilities/Source/GameplayAbilities/Public/AbilitySystemInterface.h`）。**接口要求必须实现 `GetAbilitySystemComponent()`**（纯虚函数，我核实过 `AbilitySystemInterface.h:30`） |

---

## 4. `AIDFCharacterBase.cpp` 实现

### 4.1 构造函数

```cpp
AIDFCharacterBase::AIDFCharacterBase()
{
    PrimaryActorTick.bCanEverTick = false;      // 需要 Tick 时再开（节省开销）

    // ── 网络 ──
    bReplicates = true;
    SetReplicateMovement(true);                 // ACharacter 默认已开，显式写一遍防将来换基类
    SetNetUpdateFrequency(30.f);                // ★ 注意：UE5.5 起 NetUpdateFrequency 属性已废弃，
    SetMinNetUpdateFrequency(10.f);             //   必须走 Setter（直接写属性会有 deprecation 警告）

    // ── 移动组件与朝向 ──
    if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
    {
        MoveComp->MaxWalkSpeed = WalkSpeed;
        MoveComp->MaxWalkSpeedCrouched = 200.f;
        MoveComp->MaxAcceleration = 2048.f;
        MoveComp->BrakingDecelerationWalking = 2048.f;
        MoveComp->GroundFriction = 8.f;
        MoveComp->RotationRate = FRotator(0.f, 540.f, 0.f);
        MoveComp->bOrientRotationToMovement = true;
    }
    bUseControllerRotationYaw = false;
    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll = false;

    // 本作没有跳跃（原作无跳跃机制），禁用掉避免多余状态
    JumpMaxCount = 0;

    // ── GAS ──
    AbilitySystemComponent = CreateDefaultSubobject<UIDFAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    // ASC 自身的 SetIsReplicated(true) 与 ReplicationMode 在它的构造函数里（S05 已做）

    // 注意：这里【不创建】AttributeSet。
    // 属性集由 DefaultAbilitySet 在 InitializeAbilitySystem() 里按数据资产添加（S06 的决定），
    // 这样"医生比普通求生者多一个属性集"是纯数据差异，不需要改 C++。
}
```

⚠️ **`SetNetUpdateFrequency` 而不是 `NetUpdateFrequency = 30.f`**：UE5.5 起 `AActor::NetUpdateFrequency` 被标记 `UE_DEPRECATED(5.5, "Public access to NetUpdateFrequency has been deprecated. Use SetNetUpdateFrequency() and GetNetUpdateFrequency() instead.")`（我核实过 `Actor.h:903`）。**直接写属性会出警告，某些配置下是错误。**

### 4.2 双端初始化

```cpp
void AIDFCharacterBase::BeginPlay()
{
    Super::BeginPlay();

    // 注册到 GameFramework 的初始化状态系统（S20 的角色插件靠它注入组件）
    // 注意：只在游戏世界里注册（编辑器预览世界不需要）
    UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(this);

    // 服务器上 PossessedBy 可能已经跑过了；客户端上这里还拿不到 PlayerState。
    // 所以这里【只做与 ASC 无关的初始化】，ASC 初始化交给两条专门的路径。
}

void AIDFCharacterBase::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    // 【服务器路径】被 Controller 占据时，ActorInfo 与 PlayerState 都已就绪
    InitializeAbilitySystem();
}

void AIDFCharacterBase::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();

    // 【客户端路径】PlayerState 复制到达时触发。
    // ⚠️ 这个函数【在服务器上也会被调用一次】，所以 InitializeAbilitySystem 必须幂等。
    InitializeAbilitySystem();
}

void AIDFCharacterBase::InitializeAbilitySystem()
{
    // ── 幂等守卫（关键）──
    if (bAbilitySystemInitialized)
    {
        return;
    }

    if (!IsValid(AbilitySystemComponent))
    {
        UE_LOG(LogIDFAbility, Warning, TEXT("%s：AbilitySystemComponent 无效，跳过初始化"), *GetName());
        return;
    }

    // ── ① 绑定 Owner/Avatar ──
    //   参数含义：OwnerActor（谁是"技能系统的所有者"，通常 PlayerState 或 Character）
    //             AvatarActor（谁在"物理上执行"这些能力，通常是 Character）
    //   本项目 ASC 挂 Character，所以两者都是 this。
    AbilitySystemComponent->InitAbilityActorInfo(this, this);

    // ── ②~④ 装配（顺序在 ApplyAbilitySet 内部保证）──
    ApplyAbilitySet();

    // ── ⑤ 就绪标签 ──
    AbilitySystemComponent->AddLooseGameplayTag(IDFTags::State_Ready);

    // ── ⑥ 打通标记 + 广播 ──
    bAbilitySystemInitialized = true;

    UE_LOG(LogIDFAbility, Log, TEXT("[角色] %s 初始化完成（服务器=%s，阵营=%s，生命 %.1f/%.1f）"),
        *GetName(),
        HasAuthority() ? TEXT("是") : TEXT("否"),
        *UEnum::GetValueAsString(Team),
        /* 生命值需要属性集存在，做个保护 */
        0.f, 0.f);   // ← 这里先不读属性，等确认属性集挂上后再补

    // 通知订阅者（UI、AI、表现层）
    OnCharacterReady.Broadcast(this);

    // 推进初始化状态链（S20 用；现在只是让状态机不报"未注册"）
    CheckDefaultInitialization();
}

void AIDFCharacterBase::ApplyAbilitySet()
{
    if (!IsValid(DefaultAbilitySet))
    {
        UE_LOG(LogIDFAbility, Warning, TEXT("%s：没有配置 DefaultAbilitySet，角色将没有任何能力"), *GetName());
        return;
    }

    // GiveToAbilitySystem 内部保证顺序：属性集 → GE → 能力（S06 已实现）
    DefaultAbilitySet->GiveToAbilitySystem(AbilitySystemComponent);
}
```

⚠️ **上面日志里那行 `0.f, 0.f` 是刻意的占位**——因为属性集是**通过 AbilitySet 添加**的，添加时机在 `ApplyAbilitySet()` 内部。所以想打印真实生命值，要么：

```cpp
// 方式 A：从 ASC 里查（推荐，属性集是运行时才加的）
if (const UIDFHealthSet* HealthSet = AbilitySystemComponent->GetSet<UIDFHealthSet>())
{
    UE_LOG(..., HealthSet->GetHealth(), HealthSet->GetMaxHealth());
}
```

要么在**验收时**才打印。**别在 `ApplyAbilitySet()` 之前读属性**——那时属性集还不存在，`GetSet` 返回 nullptr，解引用就崩了。这正是"顺序不能颠倒"的又一个体现。

### 4.3 `GetAbilitySystemComponent()` 与 InitState 接口

```cpp
UAbilitySystemComponent* AIDFCharacterBase::GetAbilitySystemComponent() const
{
    // 设计文档 9.1：ASC 挂 Character。
    // 但这里保留"向 PlayerState 兜底查询"的能力，为将来的"重生保留能力"留路。
    // ⚠️ 用 GetAbilitySystemComponentFromActor 这类工具时注意别造成无限递归——
    //    这里直接返回自己的成员，不走工具函数，所以安全。
    return AbilitySystemComponent;
}

FName AIDFCharacterBase::GetFeatureName() const
{
    // 本对象的"特性名"由具体角色类覆写（例如 "Doctor" / "Clown"）。
    // 基类返回 NAME_None，表示"我是 Actor 本身，不代表某个具体特性"。
    return NAME_None;
}

void AIDFCharacterBase::CheckDefaultInitialization()
{
    // 推进默认初始化状态链：Spawned → DataAvailable → DataInitialized → GameplayReady
    //
    // S07 阶段我们【只把状态推到 DataInitialized】，因为：
    //   - Spawned：Actor 已生成（引擎在注册 receiver 时自动设）
    //   - DataAvailable：PlayerState/Controller 已就绪
    //   - DataInitialized：我们的 ASC 装配完成 ← 本节做到这里
    //   - GameplayReady：等 S20 的角色插件注入完毕后由插件推进
    //
    // 注意：这依赖 InitStateOrder 已经被 RegisterInitState 注册过（见 0.2 节）。
    //       本节先实现标签与空推进，S20 再补完整的 CanChangeInitState 判断。
    if (UGameFrameworkComponentManager* Manager = GetComponentManager())
    {
        TArray<FGameplayTag> StateChain;
        StateChain.Add(IDFTags::InitState_DataInitialized);
        ContinueInitStateChain(StateChain);
    }
}
```

### 4.4 复制

```cpp
void AIDFCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AIDFCharacterBase, Team);

    // LifeStateTag 用 COND_None + REPNOTIFY_Always？不，它是普通标签，用普通复制即可。
    DOREPLIFETIME(AIDFCharacterBase, LifeStateTag);
}

void AIDFCharacterBase::OnRep_LifeStateTag()
{
    // 状态变化时的表现（动画/特效/UI）。S19 起接动画，本节先打日志
    UE_LOG(LogIDFAbility, Verbose, TEXT("[角色] %s 生命状态变为 %s"), *GetName(), *LifeStateTag.ToString());
}
```

---

## 5. `AIDFSurvivorCharacter` / `AIDFHunterCharacter`（占位基类）

本节只建**抽象占位**，让 S19/S21 的真实角色插件有明确的父类：

```cpp
// IDFSurvivorCharacter.h
UCLASS(Abstract)
class IDFCORE_API AIDFSurvivorCharacter : public AIDFCharacterBase
{
    GENERATED_BODY()
public:
    AIDFSurvivorCharacter();
    // S19 会在这里加：治疗、救援、道具栏
};
```

```cpp
// IDFSurvivorCharacter.cpp
AIDFSurvivorCharacter::AIDFSurvivorCharacter()
{
    Team = EIDFTeam::Survivor;   // ← 阵营在构造里定死，不需要蓝图配
    SetNetUpdateFrequency(30.f);
}
```

监管者同理（`Team = EIDFTeam::Hunter`）。

> **为什么 `Team` 在 C++ 构造函数里设**：它是"这个类是什么"的固有属性，不该让配置出错。蓝图里改 `Team` 会让"求生者基类却分到监管者阵营"这种荒谬状态成为可能。**能用类型表达的就不要用数据表达**——这是我们 S06 用 USTRUCT 替代平行数组的同一个原则。

---

## 6. 改造 `AIDFGasTestActor`（或直接删除）

S07 有了真正的角色基类后，`AIDFGasTestActor` 的使命结束。两个选择：

| 方案 | 说明 |
|---|---|
| **A（推荐）** | **删除** `IDFGasTestActor.h/.cpp`，把它的验证逻辑转到关卡里放的求生者/监管者蓝图上 |
| B | 保留但改成"什么都不做"的夹具 | 会留下死代码，不推荐 |

**删除后记得**：`S05` 里 `Plugins/GameFeatures/IDFCore/Content/` 下如果建过引用它的资产/关卡实例，要清理。本项目当时是直接拖进关卡的，所以**只需打开 `L_MilitaryFactory` 删掉那个 Actor 实例**。

---

## 7. 验证：本节必须用"5 窗口"来验

**本节的验证无法在单窗口完成**——双端初始化的问题**只在客户端暴露**。

### 7.1 准备：给关卡放角色

`L_MilitaryFactory` 里：
1. 放一个 `PlayerStart`（S05 已放）；
2. 在 **World Settings → GameMode Override** 里临时指定 `AIdentity5GameMode`（或保持默认），并确认 `DefaultPawnClass` 指向我们的角色——**但角色是 `Abstract`，不能直接做 DefaultPawn！**

⚠️ **这是个必须解决的实际问题**：`AIDFCharacterBase` / `AIDFSurvivorCharacter` / `AIDFHunterCharacter` 都是 `Abstract`，`AGameModeBase::DefaultPawnClass` 需要一个**可实例化**的类。

**解决办法**（二选一）：

| 方案 | 做法 | 评价 |
|---|---|---|
| **A（推荐，本节用）** | 基于 `AIDFSurvivorCharacter` 建一个蓝图 `BP_IDFTestSurvivor`（不带 Abstract），把 `DefaultAbilitySet`、`InputConfig` 在蓝图里配好；然后建一个蓝图 `BP_Identity5GameMode`（继承 `AIdentity5GameMode`），把 `DefaultPawnClass` 设为 `BP_IDFTestSurvivor` | ✅ 数据在蓝图里配（对应 S06 的数据驱动思路） |
| B | 临时去掉 `Abstract` | ❌ 会忘记改回来，且基类可实例化本身就是设计缺陷 |

**`BP_Identity5GameMode` 也要挂到关卡上**：`World Settings → GameMode Override` 选它。（这正好用上了 S03 设计的"两层 GameMode"：配置里的入口 GameMode + 地图级覆盖。）

### 7.2 自测清单

| # | 验证项 | 期望结果 |
|---|---|---|
| 1 | 编译通过 | 特别注意 `IGameFrameworkInitStateInterface` 的纯虚/默认实现是否都满足（`GetOwningActor` 有默认实现，`GetFeatureName` 我们自己覆写） |
| 2 | 服务器初始化 | 服务器窗口日志：`[角色] BP_IDFTestSurvivor_C_0 初始化完成（服务器=是，阵营=Survivor，生命 100.0/100.0）` |
| 3 | **客户端初始化**（核心） | **客户端窗口**日志同样有这一行，且 `服务器=否`、生命也是 `100.0/100.0`。**这是本节成败的唯一标准** |
| 4 | 属性集在两端都存在 | 在 `InitializeAbilitySystem` 末尾加临时调用 `AbilitySystemComponent->PrintDebugInfo()`，两端都显示 `属性集[0] = IDFHealthSet` |
| 5 | 能力在两端都授予 | 同上，两端都显示 `能力[0] = IDFTestAbility` |
| 6 | 就绪标签两端都有 | `PrintDebugInfo` 的 `当前标签` 一行含 `State.Ready` |
| 7 | 幂等生效 | 日志里 `初始化完成` **每个角色在每个端只出现一次**（若出现两次 → 守卫失效） |
| 8 | 移动同步 | 客户端移动，服务器与其他客户端能看到（S09 的正式验收，这里先粗查） |
| 9 | `stat net` 正常 | 无异常高的复制量；角色 `NetUpdateFrequency` 为 30 |

### 7.3 验证项 3 失败时的排查表（按可能性排序）

| 症状 | 根因 | 修复 |
|---|---|---|
| 客户端完全没有"初始化完成"日志 | **只写了 `PossessedBy`，没写 `OnRep_PlayerState`** | 两个都实现，且都调同一个 `InitializeAbilitySystem` |
| 有日志但生命是 `0.0/0.0` | 初始化顺序错：属性集加在 GE 之前没做（`GiveToAbilitySystem` 内部顺序），或 `InitAbilityActorInfo` 在属性集之后 | 检查 `GiveToAbilitySystem` 的三段顺序 |
| 有日志、属性对，但能力为 0 | `DefaultAbilitySet` 在客户端为空 | 检查蓝图的 `DefaultAbilitySet` 是否配了；蓝图资产要保存 |
| 服务端两次"初始化完成" | 守卫没生效（`bAbilitySystemInitialized` 被重置，或写在了错误的函数里） | 确认守卫是**函数第一行**，且是同一个 bool |
| 客户端崩溃 | `ApplyAbilitySet` 里读了还没添加的属性集 | 不要在那之前 `GetSet<T>()` 并解引用 |
| 客户端 `State.Ready` 缺失 | `AddLooseGameplayTag` 只在服务器执行（被 `if (HasAuthority())` 包住了） | 去掉那个判断——标签两端都要加 |
| PIE 里客户端报 `Ensure` 找不到 InitState 标签 | `InitStateOrder` 没注册（见 0.2 节） | 要么注册状态顺序，要么本节先不调 `ContinueInitStateChain` |

---

## 8. 常见坑汇总

| 现象 | 原因 | 处理 |
|---|---|---|
| 客户端 ASC 为空 | 只在 `PossessedBy` 初始化 | 补 `OnRep_PlayerState`，两路收敛到同一函数 |
| 初始化跑两次 | `OnRep_PlayerState` 服务器也会触发 | 加 `bAbilitySystemInitialized` 守卫 |
| 生命值在客户端为 0，但 `State.Ready` 有 | 属性集加在 GE 之后 | 顺序：InitActorInfo → AttributeSet → GE → Ability |
| 编译错 `NetUpdateFrequency is deprecated` | UE5.5 起用 Setter | `SetNetUpdateFrequency(30.f)` |
| 编译错 `IGameFrameworkInitStateInterface 有纯虚函数未实现` | 漏了 `GetFeatureName()`（它是纯虚的？—— 不是，有默认实现） | 实际要检查的是 `GetAbilitySystemComponent()`（`IAbilitySystemInterface` 的纯虚函数） |
| 编译错找不到 `Components/GameFrameworkInitStateInterface.h` | 路径写成了公共根 | 正确路径含 `Components/`；且 `Build.cs` 要有 `ModularGameplay` |
| 角色不生成 / 生成的是默认 Pawn | `DefaultPawnClass` 指向了 `Abstract` 类 | 建蓝图子类并指过去 |
| `AddLooseGameplayTag` 报"标签未注册" | 标签字符串拼错，或 `InitState` 标签没定义 | 用常量；本节新增的 4 个标签要在 `.cpp` 里定义 |
| 客户端看不到角色 | `bReplicates` 为 false（`ACharacter` 默认是 true，但换成 `AActor` 基类就会丢） | 显式 `bReplicates = true` |

---

## 9. 提交

```powershell
cd 'G:\Project\Unreal Projects\Identity5'

git add Plugins/GameFeatures/IDFCore/Source `
        Plugins/GameFeatures/IDFCore/Content `
        Content/IDF/Maps

git status --short
git commit -m "feat(character): 新增 AIDFCharacterBase 及双端 ASC 初始化（PossessedBy/OnRep_PlayerState 收敛 + 幂等守卫），补齐 InitState 标签"
```

---

## 10. 简历点与面试问答

💼 **可以这样写**：

> **GAS 双端初始化时序**：解决 GAS 在服务器/客户端唤醒时机不同的问题——服务器走 `PossessedBy`、客户端走 `OnRep_PlayerState`，两路收敛到同一个**幂等**的 `InitializeAbilitySystem()`；初始化内部严格固定顺序（`InitAbilityActorInfo` → AttributeSet → 初始 GE → 授予能力 → 就绪标签），并解释每一环颠倒会导致的静默失败（GE 找不到属性集时不报错、属性不变）。

**这一节的面试问答是整份文档里最值钱的部分，务必能脱稿讲**：

1. **"为什么客户端要写在 `OnRep_PlayerState` 里？"**
   → 客户端不会走服务器的 Possess 流程，`PossessedBy` 在客户端不被调用；客户端是在 `PlayerState`/`Controller` 复制到达时才具备初始化条件，所以用 `RepNotify` 作为触发点。

2. **"为什么要幂等？"**
   → `OnRep_PlayerState` 在服务器也会被调用一次，且客户端可能因属性重同步触发多次。非幂等会导致重复授予能力（能力列表出现多份）、重复应用初始 GE（数值翻倍）。

3. **"初始化顺序里最关键的是哪一步？"**
   → 属性集必须在应用任何 GE **之前**添加。否则 GE 找不到属性所属的 AttributeSet，**静默失败**——不报错、不警告，属性就是不变。这类问题没有日志可查，只能靠顺序保证。

4. **"ASC 挂 Character 还是 PlayerState？本项目为什么这么选？"**
   → 挂 Character 更简单，适合"无重生"的本作；挂 PlayerState 能让能力跨重生保留。我们用 `IAbilitySystemInterface` 抽象了获取入口，将来要迁移只改 `GetAbilitySystemComponent()` 与 `InitAbilityActorInfo` 的参数，调用方不受影响。

5. **"你是怎么验证客户端初始化正确的？"**
   → 用 Listen Server + 4 客户端的 5 窗口环境，在**客户端窗口的日志**里比对 `初始化完成` 那一行的阵营与生命值，并用 `PrintDebugInfo()` 比对属性集/能力/标签三份清单与服务器一致。**强调"单窗口测不出这类问题"**——这是本节方法论的核心。

---

## 11. 第 11 节（备用）：`UIDFGameplayMessageSubsystem` 的接口设计

如果 S13 之后确实需要"一对多、跨系统、按标签路由的事件广播"，按下面的接口自己实现（约 100 行）：

```cpp
/** 消息载荷基类（可留空，用于无载荷消息） */
USTRUCT(BlueprintType)
struct FIDFGameplayMessage
{
    GENERATED_BODY()
};

UCLASS()
class IDFCORE_API UIDFGameplayMessageSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    template <typename MessageType>
    void BroadcastMessage(FGameplayTag Channel, const MessageType& Message);

    template <typename MessageType>
    FDelegateHandle RegisterListener(FGameplayTag Channel,
        TFunction<void(FGameplayTag, const MessageType&)> Callback);

    void UnregisterListener(FGameplayTag Channel, FDelegateHandle Handle);

    /** 通道注册表（每个通道一个多播委托），用 FGameplayTag 做键 */
private:
    TMap<FGameplayTag, TSharedPtr<FIDFMessageRouter>> ChannelMap;
};
```

**关键设计点**：
- 用 `FGameplayTag` 做通道（自带层级，可监听父标签）；
- 订阅者用 `TFunction` 而非 `UObject` 委托（不要求订阅者一定是 UObject，但要自己管生命周期）；
- 必须**在 GameInstanceSubsystem 里**（跨关卡存活，且随 GameInstance 清理）。

**但请注意**：这个子系统在 S13（破译完成广播）之前**没有真实需求**。**不要提前实现它**——我们已经在 S06 讨论过"不要提前写没用的东西"，这里同理。
