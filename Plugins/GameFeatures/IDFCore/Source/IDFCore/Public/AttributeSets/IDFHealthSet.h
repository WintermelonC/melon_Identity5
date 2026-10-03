#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "IDFHealthSet.generated.h"

/**
 * 角色生命属性集。
 *
 * 属性设计（对应设计文档第 2 轮 9.2 节）：
 *   Health     —— 当前生命值。需要复制：客户端要显示血条、判定受伤/倒地
 *   MaxHealth  —— 最大生命值。需要复制：客户端 UI 要用
 *   Damage     —— 【Meta 属性】伤害输入。不复制：它只是 GE 传进来的瞬时值，
 *                 在 PostGameplayEffectExecute 里被消费掉后立即清零
 *   Healing    —— 【Meta 属性】治疗输入。同上
 *
 * ⚠️ Meta 属性的关键约定：它们不参与复制，且每次结算后必须清零，
 *    否则下一次结算会读到上一次的残留值（经典 bug：第一次扣 30，第二次扣 60）。
 */
UCLASS()
class IDFCORE_API UIDFHealthSet : public UAttributeSet
{
	GENERATED_BODY()
	
public:
	UIDFHealthSet();
	
	// ── 属性声明 ──
	// 所有属性都用 FGameplayAttributeData（它内部区分 BaseValue / CurrentValue，
	// 这是 GAS 能"叠加修正器"的基础 —— 不要用裸 float）。
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "IDF|Health")
	FGameplayAttributeData Health;
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "IDF|Health")
	FGameplayAttributeData MaxHealth;
	
	// Meta 属性：无 ReplicatedUsing —— 它们不复制
	UPROPERTY(BlueprintReadOnly, Category = "IDF|Health")
	FGameplayAttributeData Damage;
	
	UPROPERTY(BlueprintReadOnly, Category = "IDF|Health")
	FGameplayAttributeData Healing;
	
	// ── 访问器宏 ──
	// ATTRIBUTE_ACCESSORS_BASIC 会生成 4 个函数（已核实 AttributeSet.h:465）：
	//   static FGameplayAttribute GetHealthAttribute();   ← 给 GE 与代码定位属性用
	//   float GetHealth() const;                          ← 读当前值
	//   void  SetHealth(float);                           ← 写 BaseValue（会走 SetNumericAttributeBase）
	//   void  InitHealth(float);                          ← 初始化 BaseValue + CurrentValue（只该在初始化时用）
	ATTRIBUTE_ACCESSORS_BASIC(UIDFHealthSet, Health);
	ATTRIBUTE_ACCESSORS_BASIC(UIDFHealthSet, MaxHealth);
	ATTRIBUTE_ACCESSORS_BASIC(UIDFHealthSet, Damage);
	ATTRIBUTE_ACCESSORS_BASIC(UIDFHealthSet, Healing);
	
protected:
	// ── 网络复制 ──
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ── 看门人：只做夹取 ──
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;

	// ── 会计：做结算 ──
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	// ── RepNotify ──
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldHealth);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);
};