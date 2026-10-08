#include "Characters/IDFSurvivorCharacter.h"

AIDFSurvivorCharacter::AIDFSurvivorCharacter()
{
	Team = EIDFTeam::Survivor;
	SetNetUpdateFrequency(30.f);
}
