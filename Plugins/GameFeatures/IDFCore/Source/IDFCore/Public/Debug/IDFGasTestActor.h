#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"
#include "IDFGasTestActor.generated.h"

class UIDFHealthSet;
class UIDFAbilitySystemComponent;

/**
 * 【临时验证用】挂载 ASC + HealthSet 的最小 Actor。
 *
 * 用途：S05 阶段验证属性集注册与网络复制是否正常。
 * 生命周期：S07 建立 AIDFCharacterBase 后，本类可删除或保留为测试夹具。
 */
UCLASS()
class IDFCORE_API AIDFGasTestActor : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()
	
public:
	AIDFGasTestActor();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "IDF|GAS")
	TObjectPtr<UIDFAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	UIDFHealthSet* HealthSet;
};