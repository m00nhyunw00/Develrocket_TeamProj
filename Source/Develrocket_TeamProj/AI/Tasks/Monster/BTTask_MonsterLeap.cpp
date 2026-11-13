#include "AI/Tasks/Monster/BTTask_MonsterLeap.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Abilities/Monster/TBGameplayAbility_Monster_Leap.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/Monster/TBMonster.h"
#include "TBGameplayTags.h"

UBTTask_MonsterLeap::UBTTask_MonsterLeap()
{
	NodeName = TEXT("TB Monster Leap");
	bCreateNodeInstance = true;
	BlackboardKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(ThisClass, BlackboardKey), AActor::StaticClass());
	BlackboardKey.SelectedKeyName = TEXT("TargetActor");
}

EBTNodeResult::Type UBTTask_MonsterLeap::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const AAIController* Controller = OwnerComp.GetAIOwner();
	ATBMonster* Monster = Controller ? Cast<ATBMonster>(Controller->GetPawn()) : nullptr;
	const UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AActor* Target = Blackboard ? Cast<AActor>(Blackboard->GetValueAsObject(BlackboardKey.SelectedKeyName)) : nullptr;
	UAbilitySystemComponent* TargetASC = IsValid(Target) ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target) : nullptr;
	if (!IsValid(Monster) || !Monster->HasAuthority() || Monster->IsDead() || !IsValid(TargetASC)
		|| TargetASC->GetAvatarActor() != Target || TargetASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead))
	{
		return EBTNodeResult::Failed;
	}

	UAbilitySystemComponent* ASC = Monster->GetAbilitySystemComponent();
	const TSubclassOf<UGameplayAbility> AbilityClass = Monster->GetLeapAbilityClass();
	if (!IsValid(ASC) || ASC->GetAvatarActor() != Monster || !AbilityClass)
	{
		return EBTNodeResult::Failed;
	}
	FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(AbilityClass);
	if (!Spec || Spec->IsActive() || !ASC->AbilityActorInfo.IsValid())
	{
		return EBTNodeResult::Failed;
	}

	ActiveASC = ASC;
	ActiveTree = &OwnerComp;
	ActiveHandle = Spec->Handle;
	bEndedDuringActivation = false;
	bCancelledDuringActivation = false;
	AbilityEndedHandle = ASC->OnAbilityEnded.AddUObject(this, &ThisClass::HandleAbilityEnded);
	bActivating = true;
	FGameplayEventData Event;
	Event.EventTag = TBGameplayTags::Ability_Combat_MonsterLeap;
	Event.Instigator = Monster;
	Event.Target = Target;
	const bool bActivated = ASC->TriggerAbilityFromGameplayEvent(ActiveHandle, ASC->AbilityActorInfo.Get(), Event.EventTag, &Event, *ASC);
	bActivating = false;
	if (!bActivated || bEndedDuringActivation)
	{
		const bool bSuccess = bActivated && bEndedDuringActivation && !bCancelledDuringActivation;
		ClearAbilityBinding();
		return bSuccess ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
	}
	return EBTNodeResult::InProgress;
}

void UBTTask_MonsterLeap::HandleAbilityEnded(const FAbilityEndedData& EndedData)
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
	if (Tree) FinishLatentTask(*Tree, EndedData.bWasCancelled ? EBTNodeResult::Failed : EBTNodeResult::Succeeded);
}

EBTNodeResult::Type UBTTask_MonsterLeap::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UAbilitySystemComponent* ASC = ActiveASC.Get();
	const FGameplayAbilitySpecHandle Handle = ActiveHandle;
	ClearAbilityBinding();
	if (IsValid(ASC) && Handle.IsValid()) ASC->CancelAbilityHandle(Handle);
	return EBTNodeResult::Aborted;
}

void UBTTask_MonsterLeap::ClearAbilityBinding()
{
	if (UAbilitySystemComponent* ASC = ActiveASC.Get()) ASC->OnAbilityEnded.Remove(AbilityEndedHandle);
	AbilityEndedHandle.Reset();
	ActiveHandle = FGameplayAbilitySpecHandle();
	ActiveASC.Reset();
	ActiveTree.Reset();
}
