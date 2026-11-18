#include "AI/Tasks/Monster/BTTask_MonsterSkill.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Abilities/Monster/TBGameplayAbility_MonsterSkill.h"
#include "AI/TBMonsterSkillSet.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/Monster/TBMonster.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TBGameplayTags.h"

namespace
{
bool IsSkillInRange(const FTBMonsterSkillEntry& Skill, double DistanceSquared)
{
	return Skill.AbilityClass && FMath::IsFinite(Skill.MinRange) && FMath::IsFinite(Skill.MaxRange)
		&& Skill.MinRange >= 0.f && Skill.MaxRange > 0.f && Skill.MinRange <= Skill.MaxRange
		&& DistanceSquared >= FMath::Square(Skill.MinRange) && DistanceSquared <= FMath::Square(Skill.MaxRange)
		&& (!Skill.bUseTargetEvent || Skill.EventTag.IsValid());
}

FGameplayAbilitySpec* FindUsableSpec(UAbilitySystemComponent& ASC, const FTBMonsterSkillEntry& Skill,
	const ATBMonster* Monster, const AActor* Target)
{
	FGameplayAbilitySpec* Spec = ASC.FindAbilitySpecFromClass(Skill.AbilityClass);
	if (!Spec || Spec->IsActive() || !Spec->Ability
		|| !Spec->Ability->CanActivateAbility(Spec->Handle, ASC.AbilityActorInfo.Get()))
	{
		return nullptr;
	}

	const auto* MonsterSkill = Cast<UTBGameplayAbility_MonsterSkill>(Spec->Ability);
	if (MonsterSkill && !MonsterSkill->CanUseAgainst(Monster, Target))
	{
		return nullptr;
	}
	return Spec;
}
}

UBTTask_MonsterSkill::UBTTask_MonsterSkill()
{
	NodeName = TEXT("TB Monster Skill");
	bCreateNodeInstance = true;
	BlackboardKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(ThisClass, BlackboardKey), AActor::StaticClass());
	BlackboardKey.SelectedKeyName = TEXT("TargetActor");
}

bool UBTTask_MonsterSkill::HasUsableSkill(UBehaviorTreeComponent& OwnerComp) const
{
	const AAIController* Controller = OwnerComp.GetAIOwner();
	const ATBMonster* Monster = Controller ? Cast<ATBMonster>(Controller->GetPawn()) : nullptr;
	const UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	const AActor* Target = Blackboard ? Cast<AActor>(Blackboard->GetValueAsObject(BlackboardKey.SelectedKeyName)) : nullptr;
	if (!IsValid(Monster) || !Monster->HasAuthority() || Monster->IsDead() || !IsValid(Target))
	{
		return false;
	}

	const UTBMonsterSkillSet* SkillSet = Monster->GetSkillSet();
	UAbilitySystemComponent* ASC = Monster->GetAbilitySystemComponent();
	if (!SkillSet || !IsValid(ASC) || ASC->GetAvatarActor() != Monster || !ASC->AbilityActorInfo.IsValid())
	{
		return false;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!IsValid(TargetASC) || TargetASC->GetAvatarActor() != Target || TargetASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead))
	{
		return false;
	}

	const double DistanceSquared = FVector::DistSquared(Monster->GetActorLocation(), Target->GetActorLocation());
	return SkillSet->Skills.ContainsByPredicate([ASC, Monster, Target, DistanceSquared](const FTBMonsterSkillEntry& Skill)
	{
		return IsSkillInRange(Skill, DistanceSquared) && FindUsableSpec(*ASC, Skill, Monster, Target) != nullptr;
	});
}

EBTNodeResult::Type UBTTask_MonsterSkill::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* Controller = OwnerComp.GetAIOwner();
	ATBMonster* Monster = Controller ? Cast<ATBMonster>(Controller->GetPawn()) : nullptr;
	const UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AActor* Target = Blackboard ? Cast<AActor>(Blackboard->GetValueAsObject(BlackboardKey.SelectedKeyName)) : nullptr;
	UAbilitySystemComponent* TargetASC = IsValid(Target) ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target) : nullptr;
	if (!IsValid(Monster) || !Monster->HasAuthority() || Monster->IsDead() || !IsValid(TargetASC)
		|| TargetASC->GetAvatarActor() != Target || TargetASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead))
		return EBTNodeResult::Failed;

	UAbilitySystemComponent* ASC = Monster->GetAbilitySystemComponent();
	const UTBMonsterSkillSet* SkillSet = Monster->GetSkillSet();
	if (!IsValid(ASC) || ASC->GetAvatarActor() != Monster || !ASC->AbilityActorInfo.IsValid() || !SkillSet)
		return EBTNodeResult::Failed;

	TArray<const FTBMonsterSkillEntry*> Candidates;
	const double DistanceSquared = FVector::DistSquared(Monster->GetActorLocation(), Target->GetActorLocation());
	for (const FTBMonsterSkillEntry& Skill : SkillSet->Skills)
	{
		if (IsSkillInRange(Skill, DistanceSquared))
			Candidates.Add(&Skill);
	}
	Candidates.StableSort([](const FTBMonsterSkillEntry& A, const FTBMonsterSkillEntry& B) { return A.Priority > B.Priority; });
	for (const FTBMonsterSkillEntry* Skill : Candidates)
	{
		FGameplayAbilitySpec* Spec = FindUsableSpec(*ASC, *Skill, Monster, Target);
		if (!Spec)
			continue;
		if (Skill->bStopMovement)
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
		FGameplayEventData Event;
		Event.EventTag = Skill->EventTag;
		Event.Instigator = Monster;
		Event.Target = Target;
		const bool bActivated = Skill->bUseTargetEvent
			? ASC->TriggerAbilityFromGameplayEvent(ActiveHandle, ASC->AbilityActorInfo.Get(), Event.EventTag, &Event, *ASC)
			: ASC->TryActivateAbility(ActiveHandle);
		bActivating = false;
		if (bActivated && !bEndedDuringActivation) return EBTNodeResult::InProgress;
		const bool bSuccess = bActivated && bEndedDuringActivation && !bCancelledDuringActivation;
		ClearAbilityBinding();
		if (bSuccess) return EBTNodeResult::Succeeded;
	}
	return EBTNodeResult::Failed;
}

void UBTTask_MonsterSkill::HandleAbilityEnded(const FAbilityEndedData& EndedData)
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

EBTNodeResult::Type UBTTask_MonsterSkill::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UAbilitySystemComponent* ASC = ActiveASC.Get();
	const FGameplayAbilitySpecHandle Handle = ActiveHandle;
	ClearAbilityBinding();
	if (IsValid(ASC) && Handle.IsValid()) ASC->CancelAbilityHandle(Handle);
	return EBTNodeResult::Aborted;
}

void UBTTask_MonsterSkill::ClearAbilityBinding()
{
	if (UAbilitySystemComponent* ASC = ActiveASC.Get()) ASC->OnAbilityEnded.Remove(AbilityEndedHandle);
	AbilityEndedHandle.Reset();
	ActiveHandle = FGameplayAbilitySpecHandle();
	ActiveASC.Reset();
	ActiveTree.Reset();
}
