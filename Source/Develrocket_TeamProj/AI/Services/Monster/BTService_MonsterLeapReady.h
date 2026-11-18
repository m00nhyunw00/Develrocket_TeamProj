#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_MonsterLeapReady.generated.h"

// Reevaluate special skills during pursuit.
UCLASS()
class DEVELROCKET_TEAMPROJ_API UBTService_MonsterLeapReady : public UBTService
{
	GENERATED_BODY()
public:
	UBTService_MonsterLeapReady();

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private:
	void UpdateSkillDuringPursuit(UBehaviorTreeComponent& OwnerComp);
	bool bRequestedSkillWhileReady = false;
};
