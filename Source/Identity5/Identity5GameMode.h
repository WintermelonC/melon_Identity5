#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Identity5GameMode.generated.h"

UCLASS(Abstract, Config = Game)
class IDENTITY5_API AIdentity5GameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AIdentity5GameMode();
};