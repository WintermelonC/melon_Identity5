#include "Debug/IDFGasTestActor.h"
#include "AbilitySystem/IDFAbilitySystemComponent.h"
#include "AbilitySystem/IDFHealthSet.h"
#include "Logging/IDFCoreLog.h"

AIDFGasTestActor::AIDFGasTestActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	AbilitySystemComponent = CreateDefaultSubobject<UIDFAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	HealthSet = CreateDefaultSubobject<UIDFHealthSet>(TEXT("HealthSet"));
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
	// 2) 再把属性集注册进去
	// 顺序反了会导致属性集找不到 Owner，PostGameplayEffectExecute 里的 AbilityActorInfo 为无效
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);

		// 方式 A：AddSet<T>() —— 由 ASC 创建实例（推荐，省去自己管理指针）
		//   注意：如果构造函数里已经 CreateDefaultSubobject 了 HealthSet，
		//   这里不要再用 AddSet（会创建第二个实例），改用下面的方式 B。
		// 方式 B：AddAttributeSetSubobject(已有实例)
		AbilitySystemComponent->AddAttributeSetSubobject(HealthSet);

		UE_LOG(LogIDFAbility, Log, TEXT("[GAS 测试] %s 初始化完成（服务器=%s），生命 %.1f/%.1f"),
			*GetName(),
			HasAuthority() ? TEXT("是") : TEXT("否"),
			HealthSet->GetHealth(), HealthSet->GetMaxHealth());
	}
}

