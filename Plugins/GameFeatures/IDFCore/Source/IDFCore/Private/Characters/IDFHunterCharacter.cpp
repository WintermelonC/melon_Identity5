#include "Characters/IDFHunterCharacter.h"

AIDFHunterCharacter::AIDFHunterCharacter()
{
	Team = EIDFTeam::Hunter;
	SetNetUpdateFrequency(30.f);
}
