#include "Character/Monster/TBVampireHunter.h"

#include "AI/AIControler/Monster/TBHunterAIController.h"
#include "Character/Monster/TBHunterAnimationData.h"

ATBVampireHunter::ATBVampireHunter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MonsterBehavior = ETBMonsterBehavior::Hunter;
	AIControllerClass = ATBHunterAIController::StaticClass();
}

const UTBHunterAnimationData* ATBVampireHunter::GetPhaseAnimationData(bool bSword) const
{
	return bSword ? SwordAnimationData.Get() : RifleAnimationData.Get();
}
