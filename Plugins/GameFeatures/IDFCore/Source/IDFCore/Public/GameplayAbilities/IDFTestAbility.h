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
