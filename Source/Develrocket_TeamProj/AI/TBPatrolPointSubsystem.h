#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "TBPatrolPointSubsystem.generated.h"

class ATBPatrolPoint;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTBPatrolGraphReady);

UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBPatrolPointSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Deinitialize() override;
	UPROPERTY(BlueprintReadOnly, Category="AI|Patrol")
	bool bPatrolGraphReady = false;
	UPROPERTY(BlueprintAssignable, Category="AI|Patrol")
	FTBPatrolGraphReady OnPatrolGraphReady;
	void RequestRebuild();
	void RegisterPatrolPoint(ATBPatrolPoint* Point);
	void UnregisterPatrolPoint(ATBPatrolPoint* Point);
	
	ATBPatrolPoint * FindPatrolPoint(const FVector& FromLocation, 
		const FGameplayTag& Zone,
		float MaxDistance,
		const ATBPatrolPoint* ExcludePoint) const;
	
private:
	void BuildNextPoint();
	FTimerHandle BuildTimer;
	int32 BuildIndex = 0;
	int32 AgentIndex = 0;
	int32 CandidateIndex = 0;
	bool bBuilding = false;
	TArray<TWeakObjectPtr<ATBPatrolPoint>> BuildCandidates;
	TArray<TWeakObjectPtr<ATBPatrolPoint>> PatrolPoints;
};
