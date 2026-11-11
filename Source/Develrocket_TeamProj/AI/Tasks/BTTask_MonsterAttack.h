#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_MonsterAttack.generated.h"

struct FAbilityEndedData;

// 블랙보드 타깃에 대한 공격 어빌리티 실행 및 종료 대기
UCLASS()
class DEVELROCKET_TEAMPROJ_API UBTTask_MonsterAttack : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTTask_MonsterAttack();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	// Distance between actor origins in Unreal units (cm).
	UPROPERTY(EditAnywhere, Category = "Attack", meta = (ClampMin = "0.0", Units = "cm"))
	float AttackRange = 150.0f;

private:
	void HandleAbilityEnded(const FAbilityEndedData& EndedData);
	void ClearAbilityBinding();
	TWeakObjectPtr<class UAbilitySystemComponent> ActiveASC;
	TWeakObjectPtr<UBehaviorTreeComponent> ActiveTree;
	FGameplayAbilitySpecHandle ActiveHandle;
	FDelegateHandle AbilityEndedHandle;
	bool bActivating = false;
	bool bEndedDuringActivation = false;
	bool bCancelledDuringActivation = false;
};
