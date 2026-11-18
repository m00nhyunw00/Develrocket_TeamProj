#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_MonsterSkill.generated.h"

struct FAbilityEndedData;
class UAbilitySystemComponent;

UCLASS()
class DEVELROCKET_TEAMPROJ_API UBTTask_MonsterSkill : public UBTTask_BlackboardBase
{
	GENERATED_BODY()
public:
	UBTTask_MonsterSkill();
	bool HasUsableSkill(UBehaviorTreeComponent& OwnerComp) const;
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
