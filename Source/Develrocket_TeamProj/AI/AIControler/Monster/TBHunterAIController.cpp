#include "AI/AIControler/Monster/TBHunterAIController.h"

#include "Character/Monster/TBMonster.h"
#include "Character/Player/TBPlayer.h"

bool ATBHunterAIController::CanTargetActor(const AActor& Actor) const
{
	return &Actor != GetPawn() && (Actor.IsA<ATBPlayer>() || Actor.IsA<ATBMonster>());
}
