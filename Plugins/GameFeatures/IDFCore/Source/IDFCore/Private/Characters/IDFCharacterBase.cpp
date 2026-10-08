#include "Characters/IDFCharacterBase.h"
#include "AbilitySystemComponents/IDFAbilitySystemComponent.h"
#include "Components/GameFrameworkComponentManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTags/IDFGameplayTags.h"
#include "Logging/IDFCoreLog.h"
#include "AbilitySets/IDFAbilitySet.h"
#include "AttributeSets/IDFHealthSet.h"
#include "Net/UnrealNetwork.h"

AIDFCharacterBase::AIDFCharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;
	
	// ── 网络 ──
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(30.f);
	SetMinNetUpdateFrequency(10.f);
	
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
	
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;
	
	// 本作没有跳跃（原作无跳跃机制），禁用掉避免多余状态
	JumpMaxCount = 0;
	
	// ── GAS ──
	AbilitySystemComponent = CreateDefaultSubobject<UIDFAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
}

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

void AIDFCharacterBase::BeginPlay()
{
	Super::BeginPlay();
	
	// 注册到 GameFramework 的初始化状态系统（S20 的角色插件靠它注入组件）
	// 注意：只在游戏世界里注册（编辑器预览世界不需要）
	UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(this);
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
	
	float HealthValue = 0.f;
	float MaxHealthValue = 0.f;
	if (const UIDFHealthSet* HealthSet = AbilitySystemComponent->GetSet<UIDFHealthSet>())
	{
		HealthValue = HealthSet->GetHealth();
		MaxHealthValue = HealthSet->GetMaxHealth();
	} 
	else
	{
		UE_LOG(LogIDFAbility, Warning, TEXT("[角色] %s 找不到 IDFHealthSet —— 检查 AbilitySet 的 GrantedAttributeSets"), *GetName());
	}
	
	UE_LOG(LogIDFAbility, Log, TEXT("[角色] %s 初始化完成（服务器=%s，阵营=%s，生命 %.1f/%.1f）"),
		*GetName(),
		HasAuthority() ? TEXT("是") : TEXT("否"),
		*UEnum::GetValueAsString(Team),
		HealthValue,
		MaxHealthValue);
	
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

void AIDFCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AIDFCharacterBase, Team);
	DOREPLIFETIME(AIDFCharacterBase, LifeStateTag);
}

void AIDFCharacterBase::OnRep_LifeStateTag()
{
	// 状态变化时的表现（动画/特效/UI）。S19 起接动画，本节先打日志
	UE_LOG(LogIDFAbility, Verbose, TEXT("[角色] %s 生命状态变为 %s"), *GetName(), *LifeStateTag.ToString());
}
