#include "AbilitySystem/Abilities/Monster/VampireHunter/TBGameplayAbility_VampireHunter_Hitscan.h"

#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/Monster/TBMonster.h"
#include "Components/TBHunterCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Interface/TBHitInterface.h"
#include "TBGameplayTags.h"

namespace
{
const FName TargetActorKey(TEXT("TargetActor"));
constexpr float AimLockTime = 0.25f;
constexpr float FireTime = 0.5f;

AActor* GetHunterTarget(const ATBMonster* Monster)
{
	const AAIController* Controller = IsValid(Monster) ? Cast<AAIController>(Monster->GetController()) : nullptr;
	const UBlackboardComponent* Blackboard = Controller ? Controller->GetBlackboardComponent() : nullptr;
	return Blackboard ? Cast<AActor>(Blackboard->GetValueAsObject(TargetActorKey)) : nullptr;
}

bool IsValidShotTarget(const ATBMonster* Monster, const AActor* Target)
{
	const ATBCharacterBase* Character = Cast<ATBCharacterBase>(Target);
	const UAbilitySystemComponent* ASC = IsValid(Character) ? Character->GetAbilitySystemComponent() : nullptr;
	return IsValid(Monster) && Monster->HasAuthority() && !Monster->IsActionBlocked()
		&& IsValid(ASC) && Target != Monster && Target->GetWorld() == Monster->GetWorld()
		&& ASC->GetAvatarActor() == Target && !ASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead);
}

#if ENABLE_DRAW_DEBUG
TAutoConsoleVariable<int32> CVarDebugHunterShot(
	TEXT("tb.Debug.HunterShot"), 0, TEXT("Draw the Vampire Hunter's charge aim and hitscan traces (0: off, 1: on)."));
#endif
}

UTBGameplayAbility_VampireHunter_Hitscan::UTBGameplayAbility_VampireHunter_Hitscan()
{
	SetAssetTags(FGameplayTagContainer(TBGameplayTags::Ability_Combat));
	ActivationOwnedTags.AddTag(TBGameplayTags::State_Combat);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Stunned);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Draining);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Combat);
}

bool UTBGameplayAbility_VampireHunter_Hitscan::CanUseAgainst(const ATBMonster* Monster, const AActor* Target) const
{
	const AAIController* Controller = IsValid(Monster) ? Cast<AAIController>(Monster->GetController()) : nullptr;
	return Controller && IsValidShotTarget(Monster, Target)
		&& Monster->GetHunterCombatComponent()->CanStartRifleAttack()
		&& (MuzzleSocketName.IsNone() || (Monster->GetMesh() && Monster->GetMesh()->DoesSocketExist(MuzzleSocketName)))
		&& !CastChecked<ATBCharacterBase>(Target)->GetAbilitySystemComponent()->HasMatchingGameplayTag(TBGameplayTags::State_Invulnerable)
		&& FMath::IsFinite(ShotDamage) && ShotDamage > 0.0f
		&& FMath::IsFinite(TraceRange) && TraceRange > 0.0f
		&& FVector::DistSquared(Monster->GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(TraceRange)
		&& Controller->LineOfSightTo(Target);
}

void UTBGameplayAbility_VampireHunter_Hitscan::ActivateAbility(FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	ATBMonster* Monster = ActorInfo ? Cast<ATBMonster>(ActorInfo->AvatarActor.Get()) : nullptr;
	AActor* Target = GetHunterTarget(Monster);
	if ((TriggerEventData && TriggerEventData->Target && Target != TriggerEventData->Target.Get())
		|| !CanUseAgainst(Monster, Target))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ThirdPersonMontage = ResolveShotMontage(*Monster);
	ShotTarget = Target;
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (IsActive())
	{
		StartShotSequence();
	}
}

UAnimMontage* UTBGameplayAbility_VampireHunter_Hitscan::GetPhaseShotMontage(const ATBMonster& Monster) const
{
	return Monster.GetHunterCombatComponent()->GetRifleAttackMontage();
}

UAnimMontage* UTBGameplayAbility_VampireHunter_Hitscan::ResolveShotMontage(const ATBMonster& Monster) const
{
	if (UAnimMontage* Montage = GetPhaseShotMontage(Monster))
	{
		return Montage;
	}
	return GetClass()->GetDefaultObject<UTBGameplayAbility_VampireHunter_Hitscan>()->ThirdPersonMontage.Get();
}

void UTBGameplayAbility_VampireHunter_Hitscan::StartShotSequence()
{
	FTimerManager& Timers = GetWorld()->GetTimerManager();
	Timers.SetTimer(AimLockTimer, this, &ThisClass::LockAim, AimLockTime, false);
	Timers.SetTimer(FireTimer, this, &ThisClass::FireShot, FireTime, false);
}

ATBMonster* UTBGameplayAbility_VampireHunter_Hitscan::GetHunter() const
{
	return Cast<ATBMonster>(GetAvatarActorFromActorInfo());
}

AActor* UTBGameplayAbility_VampireHunter_Hitscan::GetCurrentShotTarget(bool bRequireStartingConditions) const
{
	AActor* Target = ShotTarget.Get();
	const ATBMonster* Monster = GetHunter();
	if (!IsActive() || !IsValidShotTarget(Monster, Target))
	{
		return nullptr;
	}
	if (bRequireStartingConditions)
	{
		return CanUseAgainst(Monster, Target) && GetHunterTarget(Monster) == Target ? Target : nullptr;
	}
	return Monster->GetHunterCombatComponent()->GetPhase() == ETBHunterPhase::Rifle ? Target : nullptr;
}

void UTBGameplayAbility_VampireHunter_Hitscan::LockAim()
{
	const ATBMonster* Monster = Cast<ATBMonster>(GetAvatarActorFromActorInfo());
	const AActor* Target = ShotTarget.Get();
	if (!IsActive() || !CanUseAgainst(Monster, Target) || GetHunterTarget(Monster) != Target)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	LockedAimLocation = Target->GetActorLocation();
#if ENABLE_DRAW_DEBUG
	if (IsShotDebugEnabled())
	{
		DrawDebugPoint(Monster->GetWorld(), LockedAimLocation.GetValue(), 12.0f, FColor::Cyan, false, 2.0f);
	}
#endif
}

void UTBGameplayAbility_VampireHunter_Hitscan::FireShot()
{
	AActor* Target = GetCurrentShotTarget();
	const bool bFired = Target && LockedAimLocation.IsSet()
		&& TraceShot(*GetHunter(), *Target, LockedAimLocation.GetValue());
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, !bFired);
}

bool UTBGameplayAbility_VampireHunter_Hitscan::IsShotDebugEnabled()
{
#if ENABLE_DRAW_DEBUG
	return CVarDebugHunterShot.GetValueOnGameThread() != 0;
#else
	return false;
#endif
}

bool UTBGameplayAbility_VampireHunter_Hitscan::TryGetShotOrigin(const ATBMonster& Monster, FVector& OutOrigin) const
{
	return UTBHunterCombatComponent::TryGetRifleShotOrigin(Monster, MuzzleSocketName, OutOrigin);
}

bool UTBGameplayAbility_VampireHunter_Hitscan::TraceShot(ATBMonster& Monster, AActor& Target,
	const FVector& AimLocation) const
{
	FVector TraceStart;
	if (!TryGetShotOrigin(Monster, TraceStart))
	{
		return false;
	}

	const FVector AimDirection = (AimLocation - TraceStart).GetSafeNormal();
	if (AimDirection.IsNearlyZero())
	{
		return false;
	}

	const FVector TraceEnd = TraceStart + AimDirection * TraceRange;
	FHitResult Hit;
	const bool bBlockingHit = UTBHunterCombatComponent::TraceRifleShot(Monster, TraceStart, TraceEnd, Hit);
	const bool bHitTarget = bBlockingHit && Hit.GetActor() == &Target;
	const FVector ImpactEnd = bBlockingHit ? Hit.ImpactPoint : TraceEnd;
	OnShotTraced(Monster, TraceStart, ImpactEnd);

#if ENABLE_DRAW_DEBUG
	if (IsShotDebugEnabled())
	{
		const FColor TraceColor = bHitTarget ? FColor::Green : (bBlockingHit ? FColor::Orange : FColor::Red);
		DrawDebugLine(Monster.GetWorld(), TraceStart, ImpactEnd, TraceColor, false, 2.0f, 0, 2.0f);
		if (bBlockingHit)
		{
			DrawDebugPoint(Monster.GetWorld(), ImpactEnd, 12.0f, TraceColor, false, 2.0f);
		}
	}
#endif

	if (bHitTarget)
	{
		ITBHitInterface::Execute_ReceiveAttackHit(&Target, ShotDamage, &Monster, Hit);
	}

	return true;
}

void UTBGameplayAbility_VampireHunter_Hitscan::EndAbility(FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		FTimerManager& Timers = World->GetTimerManager();
		Timers.ClearTimer(AimLockTimer);
		Timers.ClearTimer(FireTimer);
	}
	ShotTarget.Reset();
	LockedAimLocation.Reset();
	if (bWasCancelled && ActorInfo && !ShouldWaitForMontageEnd())
	{
		UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
		if (ASC && ASC->GetAnimatingAbility() == this)
		{
			ASC->CurrentMontageStop();
		}
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
