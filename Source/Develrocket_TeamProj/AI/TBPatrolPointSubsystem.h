#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "TBPatrolPointSubsystem.generated.h"

class ATBPatrolPoint;

UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBPatrolPointSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	void RegisterPatrolPoint(ATBPatrolPoint* Point);
	void UnregisterPatrolPoint(ATBPatrolPoint* Point);
	
	ATBPatrolPoint * FindPatrolPoint(const FVector& FromLocation, 
		const FGameplayTag& Zone,
		float MaxDistance,
		const ATBPatrolPoint* ExcludePoint) const;
	
private:
	TArray<TWeakObjectPtr<ATBPatrolPoint>> PatrolPoints;
};