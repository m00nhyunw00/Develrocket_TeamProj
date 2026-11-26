#include "AI/Tasks/Monster/BTTask_MonsterSkill.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Abilities/Monster/TBGameplayAbility_MonsterSkill.h"
#include "AbilitySystem/Abilities/Monster/TBGameplayAbility_Monster_Charge.h"
#include "AbilitySystem/Abilities/Monster/TBGameplayAbility_Monster_Leap.h"
#include "AbilitySystem/Abilities/Monster/VampireHunter/TBGameplayAbility_VampireHunter_Hitscan.h"
#include "AI/TBMonsterSkillSet.h"
#include "AI/TBCombatTokenSubsystem.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/Monster/TBMonster.h"
#include "Components/TBHunterCombatComponent.h"
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
	UTBCombatTokenSubsystem* Tokens = nullptr;
	double DistanceSquared = 0.0;

	bool Initialize(UBehaviorTreeComponent& OwnerComp, FName TargetKey)
	{
		Controller = OwnerComp.GetAIOwner();
		Monster = Controller ? Cast<ATBMonster>(Controller->GetPawn()) : nullptr;
		const UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
		Target = Blackboard ? Cast<AActor>(Blackboard->GetValueAsObject(TargetKey)) : nullptr;
		if (!IsValid(Monster) || !Monster->HasAuthority() || Monster->IsDead() || !IsValid(Target)
			|| !Monster->GetHunterCombatComponent()->CanStartRifleAttack())
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
		Tokens = Monster->GetWorld()->GetSubsystem<UTBCombatTokenSubsystem>();
		return Tokens != nullptr;
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
	bNotifyTick = true;
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

	const bool bHasUsableSkill = Context.SkillSet->Skills.ContainsByPredicate([&Context](const FTBMonsterSkillEntry& Skill)
	{
		return IsSkillInRange(Skill, Context.DistanceSquared)
			&& FindUsableSpec(*Context.ASC, Skill, Context.Monster, Context.Target);
	});
	return bHasUsableSkill && Context.Tokens->CanRequestAttack(*Context.Monster, *Context.Target);
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
			return Context.Tokens->CanRequestAttack(*Context.Monster, *Context.Target)
				? EMovingSkillResult::RequiresTask : EMovingSkillResult::Unavailable;
		}
		const FTBCombatToken Token = Context.Tokens->TryAcquire(*Context.Monster, *Context.Target, *Context.ASC, Spec->Handle);
		if (!Token.Id) return EMovingSkillResult::Unavailable;
		if (ActivateSkill(Context, *Skill, Spec->Handle))
		{
			return EMovingSkillResult::Activated;
		}
		Context.Tokens->ReleaseIfInactive(Token);
	}
	return EMovingSkillResult::Unavailable;
}

EBTNodeResult::Type UBTTask_MonsterSkill::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	RemainingRecovery = 0.0f;
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
		const FTBCombatToken Token = Context.Tokens->TryAcquire(*Context.Monster, *Context.Target, *ASC, Spec->Handle);
		if (!Token.Id) return EBTNodeResult::Failed;
		if (Skill->bStopMovement)
		{
			Context.Controller->StopMovement();
			Context.Monster->GetCharacterMovement()->StopMovementImmediately();
		}
		ActiveASC = ASC;
		ActiveTree = &OwnerComp;
		ActiveHandle = Spec->Handle;
		RecoveryAfterSkill = 0.0f;
		const ETBMonsterBehavior Behavior = Context.Monster->GetMonsterBehavior();
		if (Behavior == ETBMonsterBehavior::Boris && Spec->Ability->IsA<UTBGameplayAbility_Monster_Leap>())
		{
			RecoveryAfterSkill = 0.45f;
		}
		else if (Behavior == ETBMonsterBehavior::Demon && Spec->Ability->IsA<UTBGameplayAbility_Monster_Charge>())
		{
			RecoveryAfterSkill = 0.75f;
		}
		bEndedDuringActivation = false;
		bCancelledDuringActivation = false;
		AbilityEndedHandle = ASC->OnAbilityEnded.AddUObject(this, &ThisClass::HandleAbilityEnded);
		bActivating = true;
		const bool bActivated = ActivateSkill(Context, *Skill, ActiveHandle);
		bActivating = false;
		if (bActivated && !bEndedDuringActivation) return EBTNodeResult::InProgress;
		Context.Tokens->ReleaseIfInactive(Token);
		const bool bSuccess = bActivated && bEndedDuringActivation && !bCancelledDuringActivation;
		ClearAbilityBinding();
		if (bSuccess && RecoveryAfterSkill > 0.0f)
		{
			RemainingRecovery = RecoveryAfterSkill;
			return EBTNodeResult::InProgress;
		}
		if (bSuccess) return EBTNodeResult::Succeeded;
	}
	return EBTNodeResult::Failed;
}

void UBTTask_MonsterSkill::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	if (RemainingRecovery <= 0.0f) return;
	RemainingRecovery = FMath::Max(0.0f, RemainingRecovery - DeltaSeconds);
	if (RemainingRecovery <= 0.0f) FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
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
	if (!Tree) return;
	if (!EndedData.bWasCancelled && RecoveryAfterSkill > 0.0f)
	{
		RemainingRecovery = RecoveryAfterSkill;
		return;
	}
	FinishLatentTask(*Tree, EndedData.bWasCancelled ? EBTNodeResult::Failed : EBTNodeResult::Succeeded);
}

EBTNodeResult::Type UBTTask_MonsterSkill::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	RemainingRecovery = 0.0f;
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
