#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_MonsterAttack.generated.h"

struct FAbilityEndedData;
class ATBMonster;
class UBlackboardComponent;

// 블랙보드 타깃에 대한 공격 어빌리티 실행 및 종료 대기
UCLASS()
class DEVELROCKET_TEAMPROJ_API UBTTask_MonsterAttack : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTTask_MonsterAttack();
	bool HasUsableAttack(UBehaviorTreeComponent& OwnerComp) const;

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private:
	FGameplayAbilitySpecHandle FindUsableAttack(const ATBMonster* Monster, const UBlackboardComponent* Blackboard) const;
	EBTNodeResult::Type StartAttack(UBehaviorTreeComponent& OwnerComp, ATBMonster& Monster, const AActor& Target,
		FGameplayAbilitySpecHandle Handle);
	EBTNodeResult::Type CompleteAttack(UBehaviorTreeComponent& OwnerComp, bool bSucceeded);
	void TickBackstep(UBehaviorTreeComponent& OwnerComp, float DeltaSeconds);
	void HandleAbilityEnded(const FAbilityEndedData& EndedData);
	void ClearAbilityBinding();
	TWeakObjectPtr<class UAbilitySystemComponent> ActiveASC;
	TWeakObjectPtr<UBehaviorTreeComponent> ActiveTree;
	FGameplayAbilitySpecHandle ActiveHandle;
	FDelegateHandle AbilityEndedHandle;
	TWeakObjectPtr<AActor> PreparationTarget;
	TWeakObjectPtr<const AActor> AttackTarget;
	float PreparationTimeRemaining = 0.0f;
	enum class EBackstepState : uint8 { None, Pending, Moving };
	EBackstepState BackstepState = EBackstepState::None;
	float BackstepTimeRemaining = 0.0f;
	bool bActivating = false;
	bool bEndedDuringActivation = false;
	bool bCancelledDuringActivation = false;
};
