#include "AbilitySystemComponents//IDFAbilitySystemComponent.h"
#include "AttributeSets/IDFHealthSet.h"
#include "IDFLog/IDFCoreLog.h"

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
