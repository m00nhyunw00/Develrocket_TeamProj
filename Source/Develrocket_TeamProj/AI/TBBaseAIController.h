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

protected:
	// 가까운 후보 중 경로를 확인할 최대 개수 (0 이하면 1개).
	UPROPERTY(EditDefaultsOnly, Category = "AI|Patrol", meta = (ClampMin = "1"))
	int32 MaxPatrolCandidates = 8;
};
