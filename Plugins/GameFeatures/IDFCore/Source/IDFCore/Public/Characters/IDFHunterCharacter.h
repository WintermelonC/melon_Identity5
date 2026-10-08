#pragma once

#include "CoreMinimal.h"
#include "IDFCharacterBase.h"
#include "IDFHunterCharacter.generated.h"

UCLASS(Abstract)
class IDFCORE_API AIDFHunterCharacter : public AIDFCharacterBase
{
	GENERATED_BODY()
	
public:
	AIDFHunterCharacter();
};
