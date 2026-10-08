#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Components/GameFrameworkInitStateInterface.h"
#include "IDFCharacterBase.generated.h"

class UIDFAbilitySystemComponent;
class UIDFAbilitySet;
class UIDFInputConfig;
class UIDFInteractionComponent;
class UIDFNetClockComponent;
class UAttributeSet;

UENUM(BlueprintType)
enum class EIDFTeam : uint8
{
	None UMETA(DisplayName = "无"),
	Survivor UMETA(DisplayName = "求生者"),
	Hunter UMETA(DisplayName = "监管者"),
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FIDFOnCharacterReady, AIDFCharacterBase*, Character);

/**
 * 所有角色的基类。
 *
 * 职责（且仅此）：
 *   1) 持有并初始化 ASC 与 AttributeSet（双端都要）
 *   2) 按 AbilitySet 装配能力与初始效果
 *   3) 提供阵营/生命状态的复制与查询
 *   4) 通过 InitState 机制向 GameFeature 插件暴露"可注入"的时机（S20 用）
 *
 * 不负责：移动手感（S14）、输入具体绑定（S14/S19）、伤害结算（在 AttributeSet 里）
 */
UCLASS(Abstract)
class IDFCORE_API AIDFCharacterBase : public ACharacter, public IAbilitySystemInterface, public IGameFrameworkInitStateInterface
{
	GENERATED_BODY()
	
public:
	AIDFCharacterBase();
	
	//~ IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
	//~ IGameFrameworkInitStateInterface
	virtual FName GetFeatureName() const override;
	virtual void CheckDefaultInitialization() override;

	UFUNCTION(BlueprintPure, Category = "IDF|Team")
	bool IsSurvivor() const { return Team == EIDFTeam::Survivor; }

	UFUNCTION(BlueprintPure, Category = "IDF|Team")
	bool IsHunter() const { return Team == EIDFTeam::Hunter; }

	/** 是否为"就绪"状态（ASC 与能力装配完成）。UI 与 AI 应以此为门控 */
	UFUNCTION(BlueprintPure, Category = "IDF|Character")
	bool IsCharacterReady() const { return bAbilitySystemInitialized; }
	
public:
	/** 角色就绪时广播（本地与远端都会触发一次）。UI/表现层订阅它，而不是在 BeginPlay 抢跑 */
	UPROPERTY(BlueprintAssignable, Category = "IDF|Character")
	FIDFOnCharacterReady OnCharacterReady;
	
protected:
	virtual void BeginPlay() override;

	//~ 双端初始化入口
	virtual void PossessedBy(AController* NewController) override;   // 服务器
	virtual void OnRep_PlayerState() override;                       // 客户端

	/** 双端共用的初始化实现。必须幂等 */
	virtual void InitializeAbilitySystem();

	/** 装配 AbilitySet（属性集/GE/能力）。单独拆出来便于子类覆写 */
	virtual void ApplyAbilitySet();
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	UFUNCTION()
	void OnRep_LifeStateTag();
	
protected:
	//~ 组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "IDF|Components")
	TObjectPtr<UIDFAbilitySystemComponent> AbilitySystemComponent;

	// UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "IDF|Components")
	// TObjectPtr<UIDFInteractionComponent> InteractionComponent;   // S11 才有实现，先占位可注释

	// UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "IDF|Components")
	// TObjectPtr<UIDFNetClockComponent> NetClockComponent;         // S13 才有实现，先占位可注释

	//~ 装配配置（由蓝图子类/角色插件指定）
	UPROPERTY(EditDefaultsOnly, Category = "IDF|Ability")
	TObjectPtr<UIDFAbilitySet> DefaultAbilitySet;

	UPROPERTY(EditDefaultsOnly, Category = "IDF|Input")
	TObjectPtr<UIDFInputConfig> InputConfig;

	//~ 复制状态
	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadOnly, Category = "IDF|Team")
	EIDFTeam Team = EIDFTeam::None;

	UPROPERTY(ReplicatedUsing = OnRep_LifeStateTag, BlueprintReadOnly, Category = "IDF|State")
	FGameplayTag LifeStateTag;

	/** 幂等守卫 */
	bool bAbilitySystemInitialized = false;

	/** 移动参数默认值（S14 会调，这里先给出来） */
	UPROPERTY(EditDefaultsOnly, Category = "IDF|Movement")
	float WalkSpeed = 400.f;

	UPROPERTY(EditDefaultsOnly, Category = "IDF|Movement")
	float SprintSpeed = 550.f;
	
public:
	/**
	 * Getters and Setters
	 */
	UFUNCTION(BlueprintPure, Category = "IDF|Team")
	EIDFTeam GetTeam() const { return Team; }
};
