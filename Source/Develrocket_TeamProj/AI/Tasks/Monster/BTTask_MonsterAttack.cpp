#include "AI/Tasks/Monster/BTTask_MonsterAttack.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AIController.h"
#include "AI/AIControler/Monster/TBMonsterAIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/Monster/TBMonster.h"
#include "TBGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"

namespace
{
	constexpr float AttackPreparationSeconds = 0.65f;
	constexpr double AttackTurnRate = 360.0;
	constexpr double AttackFacingTolerance = 15.0;
	constexpr float AttackEntrySpeed = 35.0f;
	constexpr float AttackPreparationRangeMargin = 35.0f;
	constexpr float MaxBackstepTime = 0.6f;
}

UBTTask_MonsterAttack::UBTTask_MonsterAttack()
{
	NodeName = TEXT("TB Monster Attack");
	bCreateNodeInstance = true;
	bNotifyTick = true;
	bNotifyTaskFinished = true;
	BlackboardKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(ThisClass, BlackboardKey), AActor::StaticClass());
	BlackboardKey.SelectedKeyName = TEXT("TargetActor");
}

bool UBTTask_MonsterAttack::HasUsableAttack(UBehaviorTreeComponent& OwnerComp) const
{
	const AAIController* Controller = OwnerComp.GetAIOwner();
	ATBMonster* Monster = Controller ? Cast<ATBMonster>(Controller->GetPawn()) : nullptr;
	const UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!FindUsableAttack(Monster, Blackboard).IsValid()) return false;

	AActor* Target = CastChecked<AActor>(Blackboard->GetValueAsObject(BlackboardKey.SelectedKeyName));
	UTBCombatTokenSubsystem* Tokens = Monster->GetWorld()->GetSubsystem<UTBCombatTokenSubsystem>();
	return Tokens && Tokens->CanRequestAttack(*Monster, *Target);
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
		|| Monster->GetMeleeDistanceSquaredTo(*Target) > FMath::Square(MeleeRange))
	{
		return {};
	}

	if (const ATBMonsterAIController* Controller = Cast<ATBMonsterAIController>(Monster->GetController());
		Controller && Controller->WantsCombatRetreat(*Target))
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

	AActor* Target = CastChecked<AActor>(OwnerComp.GetBlackboardComponent()->GetValueAsObject(BlackboardKey.SelectedKeyName));
	UTBCombatTokenSubsystem* Tokens = Monster->GetWorld()->GetSubsystem<UTBCombatTokenSubsystem>();
	if (!Tokens) return EBTNodeResult::Failed;
	AttackToken = Tokens->TryAcquire(*Monster, *Target, *Monster->GetAbilitySystemComponent(), Handle);
	if (!AttackToken.Id) return EBTNodeResult::Failed;

	PreparationTarget = Target;
	PreparedHandle = Handle;
	PreparationTimeRemaining = AttackPreparationSeconds;
	if (UPathFollowingComponent* PathFollowing = Controller->GetPathFollowingComponent())
	{
		PathFollowing->AbortMove(*this, FPathFollowingResultFlags::OwnerFinished,
			FAIRequestID::CurrentRequest,
			Monster->IsSummonedMonster() ? EPathFollowingVelocityMode::Reset : EPathFollowingVelocityMode::Keep);
	}
	return EBTNodeResult::InProgress;
}

void UBTTask_MonsterAttack::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	if (BackstepState != EBackstepState::None)
	{
		TickBackstep(OwnerComp, DeltaSeconds);
		return;
	}

	if (PreparationTarget.IsExplicitlyNull()) return;

	AAIController* Controller = OwnerComp.GetAIOwner();
	ATBMonster* Monster = Controller ? Cast<ATBMonster>(Controller->GetPawn()) : nullptr;
	const UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	const AActor* Target = PreparationTarget.Get();
	PreparationTimeRemaining -= DeltaSeconds;
	if (!IsValid(Monster) || Monster->IsDead() || Monster->IsMovementBlocked()
		|| !IsValid(Target) || !Blackboard || Blackboard->GetValueAsObject(BlackboardKey.SelectedKeyName) != Target
		|| PreparationTimeRemaining <= 0.0f)
	{
		PreparationTarget.Reset();
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	const FVector ToTarget = Target->GetActorLocation() - Monster->GetActorLocation();
	const float AttackRange = Monster->GetMeleeAttackRange();
	const double DistanceSquared = Monster->GetMeleeDistanceSquaredTo(*Target);
	if (DistanceSquared > FMath::Square(AttackRange + AttackPreparationRangeMargin))
	{
		PreparationTarget.Reset();
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	FRotator Rotation = Monster->GetActorRotation();
	const double TargetYaw = ToTarget.SizeSquared2D() <= UE_SMALL_NUMBER ? Rotation.Yaw : ToTarget.Rotation().Yaw;
	const double MaxTurn = AttackTurnRate * DeltaSeconds;
	Rotation.Yaw += FMath::Clamp(FMath::FindDeltaAngleDegrees(Rotation.Yaw, TargetYaw), -MaxTurn, MaxTurn);
	Monster->SetActorRotation(Rotation);
	if (FMath::Abs(FMath::FindDeltaAngleDegrees(Rotation.Yaw, TargetYaw)) > AttackFacingTolerance
		|| Monster->GetVelocity().SizeSquared2D() > FMath::Square(AttackEntrySpeed)
		|| DistanceSquared > FMath::Square(AttackRange))
	{
		return;
	}

	const FGameplayAbilitySpecHandle Handle = FindUsableAttack(Monster, Blackboard);
	PreparationTarget.Reset();
	const UTBCombatTokenSubsystem* Tokens = Monster->GetWorld()->GetSubsystem<UTBCombatTokenSubsystem>();
	const EBTNodeResult::Type Result = Handle == PreparedHandle && Tokens && Tokens->IsHeld(AttackToken)
		? StartAttack(OwnerComp, *Monster, *Target, Handle) : EBTNodeResult::Failed;
	if (Result != EBTNodeResult::InProgress) FinishLatentTask(OwnerComp, Result);
}

EBTNodeResult::Type UBTTask_MonsterAttack::StartAttack(
	UBehaviorTreeComponent& OwnerComp, ATBMonster& Monster, const AActor& Target, FGameplayAbilitySpecHandle Handle)
{
	UAbilitySystemComponent* ASC = Monster.GetAbilitySystemComponent();
	Monster.GetCharacterMovement()->StopMovementImmediately();

	ActiveASC = ASC;
	ActiveTree = &OwnerComp;
	ActiveHandle = Handle;
	AttackTarget = &Target;
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
		return CompleteAttack(OwnerComp, bSuccess);
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
		const EBTNodeResult::Type Result = CompleteAttack(*Tree, !EndedData.bWasCancelled);
		if (Result != EBTNodeResult::InProgress) FinishLatentTask(*Tree, Result);
	}
}

EBTNodeResult::Type UBTTask_MonsterAttack::CompleteAttack(UBehaviorTreeComponent& OwnerComp, bool bSucceeded)
{
	const ATBMonsterAIController* Controller = Cast<ATBMonsterAIController>(OwnerComp.GetAIOwner());
	const ATBMonster* Monster = Controller ? Cast<ATBMonster>(Controller->GetPawn()) : nullptr;
	if (bSucceeded && IsValid(Monster) && Monster->GetMonsterBehavior() == ETBMonsterBehavior::Anasimus
		&& AttackTarget.IsValid())
	{
		// Wait for the ability to release its movement state before requesting the backstep.
		BackstepState = EBackstepState::Pending;
		return EBTNodeResult::InProgress;
	}

	AttackTarget.Reset();
	return bSucceeded ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
}

void UBTTask_MonsterAttack::TickBackstep(UBehaviorTreeComponent& OwnerComp, float DeltaSeconds)
{
	ATBMonsterAIController* Controller = Cast<ATBMonsterAIController>(OwnerComp.GetAIOwner());
	const ATBMonster* Monster = Controller ? Cast<ATBMonster>(Controller->GetPawn()) : nullptr;
	const UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AActor* Target = Blackboard ? Cast<AActor>(Blackboard->GetValueAsObject(BlackboardKey.SelectedKeyName)) : nullptr;
	if (IsValid(Monster) && !Monster->IsMovementBlocked() && IsValid(Target) && Target == AttackTarget.Get())
	{
		if (BackstepState == EBackstepState::Pending && Controller->TryAnasimusBackstep(*Target))
		{
			BackstepState = EBackstepState::Moving;
			BackstepTimeRemaining = MaxBackstepTime;
			return;
		}
		if (BackstepState == EBackstepState::Moving && Controller->IsAnasimusBackstepping()
			&& (BackstepTimeRemaining -= DeltaSeconds) > 0.0f)
		{
			return;
		}
	}

	if (Controller) Controller->StopAnasimusBackstep();
	AttackTarget.Reset();
	BackstepState = EBackstepState::None;
	FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
}

EBTNodeResult::Type UBTTask_MonsterAttack::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	PreparationTarget.Reset();
	if (BackstepState == EBackstepState::Moving)
	{
		if (ATBMonsterAIController* Controller = Cast<ATBMonsterAIController>(OwnerComp.GetAIOwner()))
		{
			Controller->StopAnasimusBackstep();
		}
	}
	BackstepState = EBackstepState::None;
	AttackTarget.Reset();
	UAbilitySystemComponent* ASC = ActiveASC.Get();
	const FGameplayAbilitySpecHandle Handle = ActiveHandle;
	ClearAbilityBinding();
	if (IsValid(ASC) && Handle.IsValid()) ASC->CancelAbilityHandle(Handle);
	return EBTNodeResult::Aborted;
}

void UBTTask_MonsterAttack::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	UWorld* World = OwnerComp.GetWorld();
	if (UTBCombatTokenSubsystem* Tokens = World ? World->GetSubsystem<UTBCombatTokenSubsystem>() : nullptr)
	{
		Tokens->ReleaseIfInactive(AttackToken);
	}
	AttackToken = {};
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}

void UBTTask_MonsterAttack::ClearAbilityBinding()
{
	if (UAbilitySystemComponent* ASC = ActiveASC.Get()) ASC->OnAbilityEnded.Remove(AbilityEndedHandle);
	AbilityEndedHandle.Reset();
	ActiveHandle = FGameplayAbilitySpecHandle();
	ActiveASC.Reset();
	ActiveTree.Reset();
}
