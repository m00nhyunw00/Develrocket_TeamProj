#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_MonsterSkillCooltime.generated.h"

// Reevaluate attacks and special skills during pursuit.
UCLASS()
class DEVELROCKET_TEAMPROJ_API UBTService_MonsterSkillCooltime : public UBTService
{
	GENERATED_BODY()
public:
	UBTService_MonsterSkillCooltime();

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private:
	void UpdateCombatDuringPursuit(UBehaviorTreeComponent& OwnerComp);
	int32 RequestedChildIndex = INDEX_NONE;
};
