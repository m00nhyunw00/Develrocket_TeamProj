#include "AI/Tasks/BTTask_MonsterAttack.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/Monster/TBGameplayAbility_Monster_Leap.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/Monster/TBMonster.h"
#include "Character/Player/TBPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TBGameplayTags.h"

UBTTask_MonsterAttack::UBTTask_MonsterAttack()
{
	NodeName = TEXT("TB Monster Attack");
	bCreateNodeInstance = true;
	BlackboardKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(ThisClass, BlackboardKey), AActor::StaticClass());
	BlackboardKey.SelectedKeyName = TEXT("TargetActor");
}

EBTNodeResult::Type UBTTask_MonsterAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* Controller = OwnerComp.GetAIOwner();
	ATBMonster* Monster = Controller ? Cast<ATBMonster>(Controller->GetPawn()) : nullptr;
	const UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	ATBPlayer* Target = Blackboard ? Cast<ATBPlayer>(Blackboard->GetValueAsObject(BlackboardKey.SelectedKeyName)) : nullptr;

	if (!IsValid(Monster) || !Monster->HasAuthority() || Monster->IsDead()
		|| !IsValid(Target) || Target->IsDead() || AttackRange <= 0.0f)
	{
		return EBTNodeResult::Failed;
	}

	UAbilitySystemComponent* ASC = Monster->GetAbilitySystemComponent();
	// 근접 사거리를 기준으로 근접·도약 어빌리티 선택
	const bool bUseLeap = FVector::DistSquared(Monster->GetActorLocation(), Target->GetActorLocation()) > FMath::Square(AttackRange);
	const TSubclassOf<UGameplayAbility> AbilityClass = bUseLeap
		? TSubclassOf<UGameplayAbility>(Monster->GetLeapAbilityClass())
		: Monster->GetMeleeAbilityClass();
	if (!IsValid(ASC) || ASC->GetAvatarActor() != Monster || !AbilityClass)
	{
		return EBTNodeResult::Failed;
	}

	FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(AbilityClass);
	if (!Spec || Spec->IsActive())
	{
		return EBTNodeResult::Failed;
	}
	// 도약은 착지 후보 검증 후 어빌리티에서 이동 중단
	if (!bUseLeap)
	{
		Controller->StopMovement();
		Monster->GetCharacterMovement()->StopMovementImmediately();
	}

	ActiveASC = ASC;
	ActiveTree = &OwnerComp;
	ActiveHandle = Spec->Handle;
	bEndedDuringActivation = false;
	bCancelledDuringActivation = false;
	AbilityEndedHandle = ASC->OnAbilityEnded.AddUObject(this, &ThisClass::HandleAbilityEnded);
	bActivating = true;
	// 블랙보드 타깃을 GAS 이벤트로 도약 어빌리티에 전달
	FGameplayEventData LeapEvent;
	LeapEvent.EventTag = TBGameplayTags::Ability_Combat_MonsterLeap;
	LeapEvent.Instigator = Monster;
	LeapEvent.Target = Target;
	const bool bActivated = bUseLeap
		? ASC->TriggerAbilityFromGameplayEvent(ActiveHandle, ASC->AbilityActorInfo.Get(), LeapEvent.EventTag, &LeapEvent, *ASC)
		: ASC->TryActivateAbility(ActiveHandle);
	bActivating = false;
	if (!bActivated || bEndedDuringActivation)
	{
		const bool bSuccess = bActivated && bEndedDuringActivation && !bCancelledDuringActivation;
		ClearAbilityBinding();
		return bSuccess ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
	}
	return EBTNodeResult::InProgress;
}

void UBTTask_MonsterAttack::HandleAbilityEnded(const FAbilityEndedData& EndedData)
{
	if (EndedData.AbilitySpecHandle != ActiveHandle) return;
	if (bActivating)
	{
		bEndedDuringActivation = true;
		bCancelledDuringActivation = EndedData.bWasCancelled;
		return;
	}
	UBehaviorTreeComponent* Tree = ActiveTree.Get();
	ClearAbilityBinding();
	if (Tree)
	{
		FinishLatentTask(*Tree, EndedData.bWasCancelled ? EBTNodeResult::Failed : EBTNodeResult::Succeeded);
	}
}

EBTNodeResult::Type UBTTask_MonsterAttack::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UAbilitySystemComponent* ASC = ActiveASC.Get();
	const FGameplayAbilitySpecHandle Handle = ActiveHandle;
	ClearAbilityBinding();
	if (IsValid(ASC) && Handle.IsValid()) ASC->CancelAbilityHandle(Handle);
	return EBTNodeResult::Aborted;
}

void UBTTask_MonsterAttack::ClearAbilityBinding()
{
	if (UAbilitySystemComponent* ASC = ActiveASC.Get()) ASC->OnAbilityEnded.Remove(AbilityEndedHandle);
	AbilityEndedHandle.Reset();
	ActiveHandle = FGameplayAbilitySpecHandle();
	ActiveASC.Reset();
	ActiveTree.Reset();
}
