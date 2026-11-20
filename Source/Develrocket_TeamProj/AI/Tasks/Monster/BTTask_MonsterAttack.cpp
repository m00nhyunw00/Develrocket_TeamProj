#include "AI/Tasks/Monster/BTTask_MonsterAttack.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/Monster/TBMonster.h"
#include "TBGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"

UBTTask_MonsterAttack::UBTTask_MonsterAttack()
{
	NodeName = TEXT("TB Monster Attack");
	bCreateNodeInstance = true;
	BlackboardKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(ThisClass, BlackboardKey), AActor::StaticClass());
	BlackboardKey.SelectedKeyName = TEXT("TargetActor");
}

bool UBTTask_MonsterAttack::HasUsableAttack(UBehaviorTreeComponent& OwnerComp) const
{
	const AAIController* Controller = OwnerComp.GetAIOwner();
	const ATBMonster* Monster = Controller ? Cast<ATBMonster>(Controller->GetPawn()) : nullptr;
	return FindUsableAttack(Monster, OwnerComp.GetBlackboardComponent()).IsValid();
}

FGameplayAbilitySpecHandle UBTTask_MonsterAttack::FindUsableAttack(const ATBMonster* Monster, const UBlackboardComponent* Blackboard) const
{
	const AActor* Target = Blackboard ? Cast<AActor>(Blackboard->GetValueAsObject(BlackboardKey.SelectedKeyName)) : nullptr;
	UAbilitySystemComponent* TargetASC = IsValid(Target) ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target) : nullptr;

	if (!IsValid(Monster) || !Monster->HasAuthority() || Monster->IsDead()
		|| !IsValid(TargetASC) || TargetASC->GetAvatarActor() != Target
		|| TargetASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead))
	{
		return {};
	}

	const float MeleeRange = Monster->GetMeleeAttackRange();
	if (!FMath::IsFinite(MeleeRange) || MeleeRange <= 0.0f
		|| FVector::DistSquared(Monster->GetActorLocation(), Target->GetActorLocation()) > FMath::Square(MeleeRange))
	{
		return {};
	}

	UAbilitySystemComponent* ASC = Monster->GetAbilitySystemComponent();
	const TSubclassOf<UGameplayAbility> AbilityClass = Monster->GetMeleeAbilityClass();
	if (!IsValid(ASC) || ASC->GetAvatarActor() != Monster || !ASC->AbilityActorInfo.IsValid() || !AbilityClass)
	{
		return {};
	}

	const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(AbilityClass);
	if (!Spec || !Spec->Ability || Spec->IsActive()
		|| !Spec->Ability->CanActivateAbility(Spec->Handle, ASC->AbilityActorInfo.Get()))
	{
		return {};
	}
	return Spec->Handle;
}

EBTNodeResult::Type UBTTask_MonsterAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* Controller = OwnerComp.GetAIOwner();
	ATBMonster* Monster = Controller ? Cast<ATBMonster>(Controller->GetPawn()) : nullptr;
	const FGameplayAbilitySpecHandle Handle = FindUsableAttack(Monster, OwnerComp.GetBlackboardComponent());
	if (!Handle.IsValid()) return EBTNodeResult::Failed;

	UAbilitySystemComponent* ASC = Monster->GetAbilitySystemComponent();
	Controller->StopMovement();
	Monster->GetCharacterMovement()->StopMovementImmediately();

	ActiveASC = ASC;
	ActiveTree = &OwnerComp;
	ActiveHandle = Handle;
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
