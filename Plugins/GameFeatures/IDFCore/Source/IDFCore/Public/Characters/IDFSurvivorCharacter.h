#pragma once

#include "CoreMinimal.h"
#include "IDFCharacterBase.h"
#include "IDFSurvivorCharacter.generated.h"

UCLASS(Abstract)
class IDFCORE_API AIDFSurvivorCharacter : public AIDFCharacterBase
{
	GENERATED_BODY()
	
public:
	AIDFSurvivorCharacter();
};
