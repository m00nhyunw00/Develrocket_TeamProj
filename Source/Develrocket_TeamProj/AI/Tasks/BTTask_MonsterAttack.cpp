#include "AI/Tasks/BTTask_MonsterAttack.h"

#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/Monster/TBMonster.h"
#include "Character/Player/TBPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"

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
		|| !IsValid(Target) || Target->IsDead() || AttackRange <= 0.0f
		|| FVector::DistSquared(Monster->GetActorLocation(), Target->GetActorLocation()) > FMath::Square(AttackRange))
	{
		return EBTNodeResult::Failed;
	}

	UAbilitySystemComponent* ASC = Monster->GetAbilitySystemComponent();
	const TSubclassOf<UGameplayAbility> AbilityClass = Monster->GetMeleeAbilityClass();
	if (!IsValid(ASC) || ASC->GetAvatarActor() != Monster || !AbilityClass)
	{
		return EBTNodeResult::Failed;
	}

	FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(AbilityClass);
	if (!Spec || Spec->IsActive())
	{
		return EBTNodeResult::Failed;
	}
	Controller->StopMovement();
	Monster->GetCharacterMovement()->StopMovementImmediately();

	ActiveASC = ASC;
	ActiveTree = &OwnerComp;
	ActiveHandle = Spec->Handle;
	bEndedDuringActivation = false;
	bCancelledDuringActivation = false;
	AbilityEndedHandle = ASC->OnAbilityEnded.AddUObject(this, &ThisClass::HandleAbilityEnded);
	bActivating = true;
	const bool bActivated = ASC->TryActivateAbility(ActiveHandle);
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
