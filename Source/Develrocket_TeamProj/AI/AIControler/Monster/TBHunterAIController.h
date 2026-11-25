#pragma once

#include "AI/AIControler/Monster/TBMonsterAIController.h"
#include "TBHunterAIController.generated.h"

UCLASS()
class DEVELROCKET_TEAMPROJ_API ATBHunterAIController : public ATBMonsterAIController
{
	GENERATED_BODY()

protected:
	virtual bool CanTargetActor(const AActor& Actor) const override;
};
