#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "TBBaseAIController.generated.h"

// Common parent for AI controllers; specialized behavior stays in child classes.
UCLASS(Abstract)
class DEVELROCKET_TEAMPROJ_API ATBBaseAIController : public AAIController
{
	GENERATED_BODY()

public:
	// Select a reachable actor of the supplied class within the search radius.
	UFUNCTION(BlueprintCallable, Category = "AI|Patrol")
	AActor* FindReachablePatrolPoint(TSubclassOf<AActor> PointClass, float SearchRadius) const;
};
