#include "AI/Tasks/Monster/BTTask_MonsterSkill.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Abilities/Monster/TBGameplayAbility_MonsterSkill.h"
#include "AbilitySystem/Abilities/Monster/VampireHunter/TBGameplayAbility_VampireHunter_Hitscan.h"
#include "AI/TBMonsterSkillSet.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/Monster/TBMonster.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TBGameplayTags.h"

namespace
{
struct FSkillContext
{
	AAIController* Controller = nullptr;
	ATBMonster* Monster = nullptr;
	AActor* Target = nullptr;
	UAbilitySystemComponent* ASC = nullptr;
	const UTBMonsterSkillSet* SkillSet = nullptr;
	double DistanceSquared = 0.0;

	bool Initialize(UBehaviorTreeComponent& OwnerComp, FName TargetKey)
	{
		Controller = OwnerComp.GetAIOwner();
		Monster = Controller ? Cast<ATBMonster>(Controller->GetPawn()) : nullptr;
		const UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
		Target = Blackboard ? Cast<AActor>(Blackboard->GetValueAsObject(TargetKey)) : nullptr;
		if (!IsValid(Monster) || !Monster->HasAuthority() || Monster->IsDead() || !IsValid(Target))
		{
			return false;
		}
		const UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
		if (!IsValid(TargetASC) || TargetASC->GetAvatarActor() != Target || TargetASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead))
		{
			return false;
		}
		ASC = Monster->GetAbilitySystemComponent();
		SkillSet = Monster->GetSkillSet();
		if (!SkillSet || !IsValid(ASC) || ASC->GetAvatarActor() != Monster || !ASC->AbilityActorInfo.IsValid())
		{
			return false;
		}
		DistanceSquared = FVector::DistSquared(Monster->GetActorLocation(), Target->GetActorLocation());
		return true;
	}
};

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

TArray<const FTBMonsterSkillEntry*> GetSkillsInPriorityOrder(const FSkillContext& Context)
{
	TArray<const FTBMonsterSkillEntry*> Candidates;
	for (const FTBMonsterSkillEntry& Skill : Context.SkillSet->Skills)
	{
		if (IsSkillInRange(Skill, Context.DistanceSquared))
		{
			Candidates.Add(&Skill);
		}
	}
	Candidates.StableSort([](const FTBMonsterSkillEntry& A, const FTBMonsterSkillEntry& B) { return A.Priority > B.Priority; });
	return Candidates;
}

bool ActivateSkill(const FSkillContext& Context, const FTBMonsterSkillEntry& Skill, FGameplayAbilitySpecHandle Handle)
{
	UAbilitySystemComponent& ASC = *Context.ASC;
	if (!Skill.bUseTargetEvent)
	{
		return ASC.TryActivateAbility(Handle);
	}

	FGameplayEventData Event;
	Event.EventTag = Skill.EventTag;
	Event.Instigator = Context.Monster;
	Event.Target = Context.Target;
	return ASC.TriggerAbilityFromGameplayEvent(Handle, ASC.AbilityActorInfo.Get(), Event.EventTag, &Event, ASC);
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
	FSkillContext Context;
	if (!Context.Initialize(OwnerComp, BlackboardKey.SelectedKeyName))
	{
		return false;
	}

	return Context.SkillSet->Skills.ContainsByPredicate([&Context](const FTBMonsterSkillEntry& Skill)
	{
		return IsSkillInRange(Skill, Context.DistanceSquared)
			&& FindUsableSpec(*Context.ASC, Skill, Context.Monster, Context.Target);
	});
}

UBTTask_MonsterSkill::EMovingSkillResult UBTTask_MonsterSkill::TryUseSkillDuringHunterMovement(UBehaviorTreeComponent& OwnerComp) const
{
	FSkillContext Context;
	if (!Context.Initialize(OwnerComp, BlackboardKey.SelectedKeyName))
	{
		return EMovingSkillResult::Unavailable;
	}

	for (const FTBMonsterSkillEntry* Skill : GetSkillsInPriorityOrder(Context))
	{
		const FGameplayAbilitySpec* Spec = FindUsableSpec(*Context.ASC, *Skill, Context.Monster, Context.Target);
		if (!Spec)
		{
			continue;
		}
		if (Skill->bStopMovement || !Spec->Ability->IsA<UTBGameplayAbility_VampireHunter_Hitscan>())
		{
			return EMovingSkillResult::RequiresTask;
		}
		if (ActivateSkill(Context, *Skill, Spec->Handle))
		{
			return EMovingSkillResult::Activated;
		}
	}
	return EMovingSkillResult::Unavailable;
}

EBTNodeResult::Type UBTTask_MonsterSkill::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FSkillContext Context;
	if (!Context.Initialize(OwnerComp, BlackboardKey.SelectedKeyName))
	{
		return EBTNodeResult::Failed;
	}

	UAbilitySystemComponent* ASC = Context.ASC;
	for (const FTBMonsterSkillEntry* Skill : GetSkillsInPriorityOrder(Context))
	{
		FGameplayAbilitySpec* Spec = FindUsableSpec(*ASC, *Skill, Context.Monster, Context.Target);
		if (!Spec)
			continue;
		if (Skill->bStopMovement)
		{
			Context.Controller->StopMovement();
			Context.Monster->GetCharacterMovement()->StopMovementImmediately();
		}
		ActiveASC = ASC;
		ActiveTree = &OwnerComp;
		ActiveHandle = Spec->Handle;
		bEndedDuringActivation = false;
		bCancelledDuringActivation = false;
		AbilityEndedHandle = ASC->OnAbilityEnded.AddUObject(this, &ThisClass::HandleAbilityEnded);
		bActivating = true;
		const bool bActivated = ActivateSkill(Context, *Skill, ActiveHandle);
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
