#include "AbilitySets/IDFAbilitySet.h"
#include "Logging/IDFCoreLog.h"
#include "AbilitySystemComponent.h"

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
		if (!SetToGrant.AttributeSet)
		{
			continue;
		}
		
		UAttributeSet* NewSet = NewObject<UAttributeSet>(ASC, SetToGrant.AttributeSet);
		ASC->AddSpawnedAttribute(NewSet);
	}
	
	// ── 第 2 步：应用初始 GameplayEffect ──
	//   典型用途：GE_Init_BaseAttributes（把 MaxHealth 之类初始化成配置值）。
	//   注意 MakeEffectContext() → MakeOutgoingSpec() 的固定套路：
	//   Context 描述"这个效果从哪来"，Spec 是"这次执行的实例"。
	for (const FIDFAbilitySet_GameplayEffect& EffectToGrant : GrantedEffects)
	{
		if (!EffectToGrant.Effect)
		{
			continue;
		}
		
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(ASC->GetAvatarActor());
		
		const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(EffectToGrant.Effect, EffectToGrant.Level, Context);
		
		if (SpecHandle.IsValid())
		{
			ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}
	}
	
	if (!ASC->IsOwnerActorAuthoritative())
	{
		UE_LOG(LogIDFAbility, Verbose, TEXT("GiveToAbilitySystem：非权威端（客户端），跳过能力授予，等待服务器复制"));
		return;
	}
	
	// ── 第 3 步：授予能力（放最后，因为能力的激活条件可能依赖属性/标签已就绪）──
	for (const FIDFAbilitySet_Ability& AbilityToGrant : GrantedAbilities)
	{
		if (!AbilityToGrant.Ability)
		{
			continue;
		}
		
		// 输入标签走 DynamicSpecSourceTags
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
