#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "IDFInputConfig.generated.h"

class UInputMappingContext;
class UInputAction;

USTRUCT(BlueprintType)
struct IDFCORE_API FIDFInputAction
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "IDF|Input")
	TObjectPtr<UInputAction> InputAction;

	UPROPERTY(EditDefaultsOnly, Category = "IDF|Input", meta = (Categories = "Input"))
	FGameplayTag InputTag;
};

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
	TArray<FIDFInputAction> AbilityInputActions;
};
