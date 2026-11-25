#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Services/BTService_BlackboardBase.h"
#include "BTService_UpdateFleeLocation.generated.h"

UCLASS()
class DEVELROCKET_TEAMPROJ_API UBTService_UpdateFleeLocation : public UBTService_BlackboardBase
{
	GENERATED_BODY()
	
public:
	UBTService_UpdateFleeLocation();
	
protected:
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector ThreatKey;
	
	UPROPERTY(EditAnywhere, Category = "Flee", meta=(Units = "cm"))
	float FleeDistance = 800.f;
	
	UPROPERTY(EditAnywhere, Category = "Flee", meta=(Units = "cm"))
	float ProjectExtent = 300.f;
	
	UPROPERTY(EditAnywhere, Category = "Flee", meta = (Units = "cm"))
	float RandomFallbackDistance = 150.f;
	
};