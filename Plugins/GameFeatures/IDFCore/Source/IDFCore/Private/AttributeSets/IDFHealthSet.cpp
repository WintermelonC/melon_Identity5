#include "AttributeSets/IDFHealthSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffectExtension.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"
#include "Logging/IDFCoreLog.h"

UIDFHealthSet::UIDFHealthSet()
{
	// 用 Init 系列初始化"基础值"。
	// 为什么不直接在头文件里写 = 100.f？
	//   因为 FGameplayAttributeData 不是普通 float，它有独立的 BaseValue/CurrentValue。
	//   用 InitXxx() 才能同时设置两者（ATTRIBUTE_ACCESSORS_BASIC 生成的 Initter）。
	InitMaxHealth(100.f);
	InitHealth(100.f);

	InitDamage(0.f);
	InitHealing(0.f);
}

void UIDFHealthSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	// ⚠️ AttributeSet 的属性一律用 DOREPLIFETIME_CONDITION_NOTIFY + REPNOTIFY_Always
	//    原因：最终 CurrentValue 可能没变但 BaseValue 变了（buff/debuff 抵消），
	//          漏掉这次同步会导致客户端后续计算基准错误。
	DOREPLIFETIME_CONDITION_NOTIFY(UIDFHealthSet, Health,    COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UIDFHealthSet, MaxHealth, COND_None, REPNOTIFY_Always);
}

void UIDFHealthSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	
	// 只做夹取。注意这里读的是 GetMaxHealth()（当前值），不是 GetMaxHealthAttribute()。
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
}

void UIDFHealthSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	
	// 规则必须与 PreAttributeChange 保持一致，否则会出现
	// "BaseValue 超出范围但 CurrentValue 被夹住"的不一致状态。
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
}

void UIDFHealthSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	
	// ── 第一步：取出"谁打了谁"的上下文 ──
	// Data 里三个成员（已核实 GameplayEffectExtension.h:26-29）：
	//   const FGameplayEffectSpec& EffectSpec  —— 这次效果的数据（含 SetByCaller 表）
	//   FGameplayModifierEvaluatedData& EvaluatedData —— 本次被计算的修正（含 Magnitude）
	//   UAbilitySystemComponent& Target        —— 被施加者
	FGameplayEffectContextHandle Context = Data.EffectSpec.GetContext();
	UAbilitySystemComponent* SourceASC = Context.GetOriginalInstigatorAbilitySystemComponent();
	const AActor* SourceActor = SourceASC ? SourceASC->GetAvatarActor() : nullptr;
	
	// 被施加者的 Controller / Character（可能为空：例如伤害来自环境陷阱）
	AController* TargetController = nullptr;
	ACharacter* TargetCharacter = nullptr;
	if (Data.Target.AbilityActorInfo.IsValid() && Data.Target.AbilityActorInfo->AvatarActor.IsValid())
	{
		AActor* TargetActor = Data.Target.AbilityActorInfo->AvatarActor.Get();
		TargetController = Data.Target.AbilityActorInfo->PlayerController.Get();
		TargetCharacter = Cast<ACharacter>(TargetActor);
	}
	
	// ── 第二步：结算 Damage ──
	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		// 伤害值来源：优先取 SetByCaller（GE 里配置的 "Data.Damage"），
		// 否则退化为本修正的 Magnitude。
		//
		// ⚠️ 已核实：UE5.8 的 UAbilitySystemComponent 上【没有】GetSetByCallerMagnitude()。
		//    它现在只在 FGameplayEffectSpec 上（GameplayEffect.h:1120/1123）：
		//      float GetSetByCallerMagnitude(FGameplayTag DataTag, bool WarnIfNotFound = true,
		//                                    float DefaultIfNotFound = 0.f) const;
		//    请先把 "Data.Damage" 标签加进 IDFGameplayTags（S06 会做），
		//    本节先用 Magnitude 跑通链路。
		const float LocalDamage = Data.EvaluatedData.Magnitude;
		
		// 应用伤害：注意这里是"减"，且 SetHealth 会走 PreAttributeChange 夹取
		SetHealth(FMath::Clamp(GetHealth() - LocalDamage, 0.f, GetMaxHealth()));
		
		UE_LOG(LogIDFAttribute, Verbose, TEXT("[生命] %s 受到 %.1f 伤害，剩余 %.1f / %.1f（来源：%s）"),
			TargetCharacter ? *TargetCharacter->GetName() : TEXT("<未知>"),
			LocalDamage,
			GetHealth(),
			GetMaxHealth(),
			SourceActor ? *SourceActor->GetName() : TEXT("<环境>"));
		
		// 触发生命变化事件（让 UI / 表现层订阅，而不是让 UI 直接轮询属性）
		// 这里先只打日志，S07 接入 UI 时再换成广播 GameplayMessage
		if (GetHealth() <= 0.f)
		{
			UE_LOG(LogIDFAttribute, Log, TEXT("[生命] %s 生命归零"),
				TargetCharacter ? *TargetCharacter->GetName() : TEXT("<未知>"));
		}
	}
	
	// ── 第三步：结算 Healing ──
	if (Data.EvaluatedData.Attribute == GetHealingAttribute())
	{
		const float LocalHealing = Data.EvaluatedData.Magnitude;
		SetHealth(FMath::Clamp(GetHealth() + LocalHealing, 0.f, GetMaxHealth()));
		
		UE_LOG(LogIDFAttribute, Verbose, TEXT("[生命] 治疗 %.1f，当前 %.1f / %.1f"),
			LocalHealing, GetHealth(), GetMaxHealth());
	}
	
	// ── 第四步（关键）：清空 Meta 属性 ──
	// 不清零的后果：下一次 GE 执行时，如果它没有写入 Damage，
	// PostGameplayEffectExecute 会读到上次的残留值 → 重复扣血。
	// 这是 GAS 新手最经典的一个 bug。
	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		SetDamage(0.f);
	}
	if (Data.EvaluatedData.Attribute == GetHealingAttribute())
	{
		SetHealing(0.f);
	}
}

void UIDFHealthSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	// GAMEPLAYATTRIBUTE_REPNOTIFY 做两件事：
	//   1) 找到 Health 对应的 FProperty
	//   2) 调用 ASC->SetBaseAttributeValueFromReplication(...)
	//      让 GAS 的聚合器知道"这个值是被网络同步过来的"，从而正确更新 CurrentValue
	// 参数顺序：(本类, 属性名, 旧值)
	GAMEPLAYATTRIBUTE_REPNOTIFY(UIDFHealthSet, Health, OldHealth);
}

void UIDFHealthSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UIDFHealthSet, MaxHealth, OldMaxHealth);
}
