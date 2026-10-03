#include "GameplayAbilities/IDFTestAbility.h"
#include "AbilitySystemComponent.h"
#include "GameplayTags/IDFGameplayTags.h"

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
