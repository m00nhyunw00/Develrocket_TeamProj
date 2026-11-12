#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_MonsterLeap.generated.h"

struct FAbilityEndedData;
class UAbilitySystemComponent;

// Activates the monster's leap ability using the selected target.
UCLASS()
class DEVELROCKET_TEAMPROJ_API UBTTask_MonsterLeap : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTTask_MonsterLeap();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private:
	void HandleAbilityEnded(const FAbilityEndedData& EndedData);
	void ClearAbilityBinding();
	TWeakObjectPtr<UAbilitySystemComponent> ActiveASC;
	TWeakObjectPtr<UBehaviorTreeComponent> ActiveTree;
	FGameplayAbilitySpecHandle ActiveHandle;
	FDelegateHandle AbilityEndedHandle;
	bool bActivating = false;
	bool bEndedDuringActivation = false;
	bool bCancelledDuringActivation = false;
};
