#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType.h"
#include "BTService_MonsterLeapReady.generated.h"

// Update a Blackboard flag so a higher-priority leap branch can interrupt pursuit.
UCLASS()
class DEVELROCKET_TEAMPROJ_API UBTService_MonsterLeapReady : public UBTService
{
	GENERATED_BODY()
public:
	UBTService_MonsterLeapReady();
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	UPROPERTY(EditAnywhere, Category="Blackboard")
	FBlackboardKeySelector TargetKey;

	UPROPERTY(EditAnywhere, Category="Blackboard")
	FBlackboardKeySelector LeapReadyKey;
};
