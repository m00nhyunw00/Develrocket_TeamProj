#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_MonsterCharge.generated.h"

struct FAbilityEndedData;
class UAbilitySystemComponent;
class UTBGameplayAbility_Monster_Charge;

// Activates a configured charge ability with the blackboard target.
UCLASS()
class DEVELROCKET_TEAMPROJ_API UBTTask_MonsterCharge : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTTask_MonsterCharge();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
	UPROPERTY(EditAnywhere, Category = "Charge")
	TSubclassOf<UTBGameplayAbility_Monster_Charge> AbilityClass;
	UPROPERTY(EditAnywhere, Category = "Charge", meta = (ClampMin = "1.0", Units = "cm"))
	float ActivationRange = 1200.f;

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
