#include "AbilitySystem/Abilities/Monster/VampireHunter/TBGameplayAbility_VampireHunter_Sword.h"

#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Character/Monster/TBMonster.h"
#include "Components/CapsuleComponent.h"
#include "Components/TBHunterCombatComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "TBGameplayTags.h"
#include "TimerManager.h"

namespace
{
	constexpr double ApproachExtraReach = 130.0;
	constexpr double ApproachTimeout = 0.8;
	constexpr double SwordReach = 95.0;
	constexpr double MaxStrikeHeightDifference = 125.0;
	constexpr double RecoveryTime = 0.65;
	constexpr float SlashDamage = 21.f;
	constexpr float ApproachDamage = 29.f;

	double GetStrikeRange(const ATBMonster& Hunter, const AActor& Victim)
	{
		return Hunter.GetSimpleCollisionRadius() + Victim.GetSimpleCollisionRadius() + SwordReach;
	}

	bool IsLivingTarget(const ATBMonster& Hunter, const AActor& Victim)
	{
		const ATBCharacterBase* Character = Cast<ATBCharacterBase>(&Victim);
		const UAbilitySystemComponent* ASC = IsValid(Character) ? Character->GetAbilitySystemComponent() : nullptr;
		return &Hunter != &Victim && Hunter.GetWorld() == Victim.GetWorld() && IsValid(ASC)
			&& ASC->GetAvatarActor() == &Victim && !ASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead);
	}
}

UTBGameplayAbility_VampireHunter_Sword::UTBGameplayAbility_VampireHunter_Sword()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	SetAssetTags(FGameplayTagContainer(TBGameplayTags::Ability_Combat_Melee));
	ActivationOwnedTags.AddTag(TBGameplayTags::State_Combat);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Combat);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Stunned);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Draining);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_MovementBlocked);
}

bool UTBGameplayAbility_VampireHunter_Sword::CanStrike(const ATBMonster& Hunter, const AActor& Victim) const
{
	const AAIController* Controller = Cast<AAIController>(Hunter.GetController());
	const FVector Offset = Victim.GetActorLocation() - Hunter.GetActorLocation();
	return Hunter.HasAuthority() && Controller && !Hunter.IsActionBlocked() && IsLivingTarget(Hunter, Victim)
		&& Offset.SizeSquared2D() <= FMath::Square(GetStrikeRange(Hunter, Victim) + ApproachExtraReach)
		&& FMath::Abs(Offset.Z) <= MaxStrikeHeightDifference
		&& Controller->LineOfSightTo(&Victim);
}

void UTBGameplayAbility_VampireHunter_Sword::ActivateAbility(FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	ATBMonster* Hunter = ActorInfo ? Cast<ATBMonster>(ActorInfo->AvatarActor.Get()) : nullptr;
	const AActor* Victim = TriggerEventData ? TriggerEventData->Target.Get() : nullptr;
	UTBHunterCombatComponent* Combat = Hunter ? Hunter->GetHunterCombatComponent() : nullptr;
	if (!Combat || Combat->GetPhase() != ETBHunterPhase::Sword || !IsValid(Victim)
		|| !CanStrike(*Hunter, *Victim) || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Target = const_cast<AActor*>(Victim);
	bApproachStrike = FVector::DistSquared2D(Hunter->GetActorLocation(), Victim->GetActorLocation()) > FMath::Square(GetStrikeRange(*Hunter, *Victim));
	if (bApproachStrike)
	{
		AAIController* Controller = CastChecked<AAIController>(Hunter->GetController());
		FAIMoveRequest Move(Target.Get());
		Move.SetAcceptanceRadius(GetStrikeRange(*Hunter, *Victim) - 15.0).SetAllowPartialPath(false)
			.SetReachTestIncludesAgentRadius(false).SetReachTestIncludesGoalRadius(false).SetCanStrafe(true);
		const FPathFollowingRequestResult Result = Controller->MoveTo(Move);
		if (Result.Code == EPathFollowingRequestResult::Failed)
		{
			Combat->DeferSwordAttackRetry();
			FinishAttack(true);
			return;
		}
		AttackMoveId = Result.MoveId;
		Combat->SetSwordAnimation(ETBHunterSwordAnimation::Run);
		Stage = EStage::Approach;
		StageStartedAt = GetWorld()->GetTimeSeconds();
	}
	else
	{
		StartSwing(EStage::FirstSwing, *Hunter, *Victim, *Combat);
	}
	GetWorld()->GetTimerManager().SetTimer(AttackTimer, this, &ThisClass::UpdateAttack, 0.04f, true);
}

void UTBGameplayAbility_VampireHunter_Sword::UpdateAttack()
{
	ATBMonster* Hunter = Cast<ATBMonster>(GetAvatarActorFromActorInfo());
	AActor* Victim = Target.Get();
	if (!IsValid(Hunter) || !IsValid(Victim) || Hunter->IsActionBlocked() || !IsLivingTarget(*Hunter, *Victim))
	{
		FinishAttack(true);
		return;
	}
	UTBHunterCombatComponent& Combat = *Hunter->GetHunterCombatComponent();
	const double Elapsed = GetWorld()->GetTimeSeconds() - StageStartedAt;
	if (Stage == EStage::Approach)
	{
		if (FVector::DistSquared2D(Hunter->GetActorLocation(), Victim->GetActorLocation()) <= FMath::Square(GetStrikeRange(*Hunter, *Victim)))
		{
			StartSwing(EStage::FirstSwing, *Hunter, *Victim, Combat);
		}
		else if (Elapsed >= ApproachTimeout)
		{
			Combat.DeferSwordAttackRetry();
			FinishAttack(true);
		}
		return;
	}
	if (Stage == EStage::Recovery)
	{
		if (Elapsed >= RecoveryTime) FinishAttack(false);
		return;
	}
	if (!bHitChecked && Elapsed >= SwingHitTime)
	{
		bHitChecked = true;
		TryHit(*Hunter, *Victim);
		if (!IsActive()) return;
	}
	if (Elapsed >= SwingDuration)
	{
		if (Stage == EStage::FirstSwing && !bApproachStrike) StartSwing(EStage::SecondSwing, *Hunter, *Victim, Combat);
		else StartRecovery(*Hunter, *Victim, Combat);
	}
}

void UTBGameplayAbility_VampireHunter_Sword::StartSwing(EStage Swing, ATBMonster& Hunter,
	const AActor& Victim, UTBHunterCombatComponent& Combat)
{
	StopAttackMove(Hunter);
	if (AController* Controller = Hunter.GetController()) Controller->StopMovement();
	Hunter.GetCharacterMovement()->StopMovementImmediately();
	const FVector Toward = (Victim.GetActorLocation() - Hunter.GetActorLocation()).GetSafeNormal2D();
	if (!Toward.IsNearlyZero()) Hunter.SetActorRotation(Toward.Rotation());

	const ETBHunterSwordAnimation Animation = bApproachStrike ? ETBHunterSwordAnimation::ApproachSlash
		: (Swing == EStage::FirstSwing ? ETBHunterSwordAnimation::SlashOne : ETBHunterSwordAnimation::SlashTwo);
	Stage = Swing;
	StageStartedAt = GetWorld()->GetTimeSeconds();
	SwingDuration = FMath::Max(0.45f, Combat.GetSwordAnimationDuration(Animation));
	SwingHitTime = SwingDuration * Combat.GetSwordHitFraction(Animation);
	bHitChecked = false;
	Combat.SetSwordAnimation(Animation);
}

void UTBGameplayAbility_VampireHunter_Sword::TryHit(ATBMonster& Hunter, AActor& Victim) const
{
	const FVector Origin = Hunter.GetActorLocation();
	const FVector Offset = Victim.GetActorLocation() - Origin;
	if (Offset.SizeSquared2D() > FMath::Square(GetStrikeRange(Hunter, Victim)) || FMath::Abs(Offset.Z) > MaxStrikeHeightDifference
		|| FVector::DotProduct(Hunter.GetActorForwardVector(), Offset.GetSafeNormal2D()) < 0.25) return;

	FCollisionQueryParams Query(SCENE_QUERY_STAT(HunterSword), false, &Hunter);
	Query.AddIgnoredActor(&Victim);
	if (Hunter.GetWorld()->LineTraceTestByChannel(Origin, Victim.GetActorLocation(), ECC_Visibility, Query)) return;
	const auto* TargetCharacter = CastChecked<ATBCharacterBase>(&Victim);
	const FVector Impact = Victim.GetActorLocation();
	const FHitResult Hit(&Victim, TargetCharacter->GetCapsuleComponent(), Impact, -Offset.GetSafeNormal());
	Hunter.ApplyMeleeDamage(Hit, bApproachStrike ? ApproachDamage : SlashDamage);
}

void UTBGameplayAbility_VampireHunter_Sword::StartRecovery(ATBMonster& Hunter, const AActor& Victim, UTBHunterCombatComponent& Combat)
{
	StopAttackMove(Hunter);
	Hunter.GetCharacterMovement()->StopMovementImmediately();
	Stage = EStage::Recovery;
	StageStartedAt = GetWorld()->GetTimeSeconds();
	Combat.SetSwordAnimation(ETBHunterSwordAnimation::Idle);
	if (bApproachStrike) return;

	AAIController* Controller = Cast<AAIController>(Hunter.GetController());
	const UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(Hunter.GetWorld());
	if (!Controller || !Nav) return;
	const FVector Origin = Controller->GetNavAgentLocation();
	const FVector Away = (Origin - Victim.GetActorLocation()).GetSafeNormal2D();
	FNavLocation Goal;
	FVector BlockedLocation;
	if (Away.IsNearlyZero() || !Nav->ProjectPointToNavigation(Origin + Away * 130.0, Goal, FVector(35.0, 35.0, 90.0))
		|| FVector::DotProduct(Goal.Location - Origin, Away) < 70.0
		|| UNavigationSystemV1::NavigationRaycast(Controller, Origin, Goal.Location, BlockedLocation, nullptr, Controller)) return;

	FAIMoveRequest Move(Goal.Location);
	Move.SetAcceptanceRadius(18.f).SetAllowPartialPath(false).SetCanStrafe(true)
		.SetReachTestIncludesAgentRadius(false).SetReachTestIncludesGoalRadius(false);
	const FPathFollowingRequestResult Result = Controller->MoveTo(Move);
	if (Result.Code == EPathFollowingRequestResult::RequestSuccessful)
	{
		AttackMoveId = Result.MoveId;
		Combat.SetSwordAnimation(ETBHunterSwordAnimation::Backstep);
	}
}

void UTBGameplayAbility_VampireHunter_Sword::StopAttackMove(ATBMonster& Hunter)
{
	if (AAIController* Controller = Cast<AAIController>(Hunter.GetController());
		Controller && AttackMoveId.IsValid() && Controller->GetCurrentMoveRequestID() == AttackMoveId)
	{
		Controller->StopMovement();
	}
	AttackMoveId = FAIRequestID::InvalidRequest;
}

void UTBGameplayAbility_VampireHunter_Sword::FinishAttack(bool bCancelled)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bCancelled);
}

void UTBGameplayAbility_VampireHunter_Sword::EndAbility(FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(AttackTimer);
	if (ATBMonster* Hunter = Cast<ATBMonster>(GetAvatarActorFromActorInfo()))
	{
		StopAttackMove(*Hunter);
		Hunter->GetHunterCombatComponent()->SetSwordAnimation(ETBHunterSwordAnimation::Idle);
	}
	Target.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
