#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "IDFAbilitySet.generated.h"

struct FGameplayAbilitySpecHandle;
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
UCLASS(Blueprintable)
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
	void GiveToAbilitySystem(UAbilitySystemComponent* ASC, TArray<FGameplayAbilitySpecHandle>* OutHandles = nullptr) const;
	
	UPROPERTY(EditDefaultsOnly, Category = "IDF|Abilities")
	TArray<FIDFAbilitySet_Ability> GrantedAbilities;

	UPROPERTY(EditDefaultsOnly, Category = "IDF|Effects")
	TArray<FIDFAbilitySet_GameplayEffect> GrantedEffects;

	UPROPERTY(EditDefaultsOnly, Category = "IDF|Attributes")
	TArray<FIDFAbilitySet_AttributeSet> GrantedAttributeSets;
};