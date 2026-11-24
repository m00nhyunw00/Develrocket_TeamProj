#include "AbilitySystem/Abilities/Monster/VampireHunter/TBGameplayAbility_VampireHunter_Hitscan.h"

#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/Monster/TBMonster.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Interface/TBHitInterface.h"
#include "TBGameplayTags.h"

namespace
{
const FName TargetActorKey(TEXT("TargetActor"));
constexpr ECollisionChannel HunterHitscanTraceChannel = ECC_GameTraceChannel2;
constexpr float AimLockTime = 0.25f;
constexpr float FireTime = 0.5f;

AActor* GetHunterTarget(const ATBMonster* Monster)
{
	const AAIController* Controller = IsValid(Monster) ? Cast<AAIController>(Monster->GetController()) : nullptr;
	const UBlackboardComponent* Blackboard = Controller ? Controller->GetBlackboardComponent() : nullptr;
	return Blackboard ? Cast<AActor>(Blackboard->GetValueAsObject(TargetActorKey)) : nullptr;
}

#if ENABLE_DRAW_DEBUG
TAutoConsoleVariable<int32> CVarDebugHunterShot(
	TEXT("tb.Debug.HunterShot"), 0, TEXT("Draw the Vampire Hunter's hitscan trace (0: off, 1: on)."));
#endif
}

UTBGameplayAbility_VampireHunter_Hitscan::UTBGameplayAbility_VampireHunter_Hitscan()
{
	SetAssetTags(FGameplayTagContainer(TBGameplayTags::Ability_Combat));
	ActivationOwnedTags.AddTag(TBGameplayTags::State_Combat);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Stunned);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Draining);
}

bool UTBGameplayAbility_VampireHunter_Hitscan::CanUseAgainst(const ATBMonster* Monster, const AActor* Target) const
{
	const ATBCharacterBase* Character = Cast<ATBCharacterBase>(Target);
	const UAbilitySystemComponent* TargetASC = IsValid(Character) ? Character->GetAbilitySystemComponent() : nullptr;
	return IsValid(Monster) && Monster->HasAuthority() && !Monster->IsActionBlocked()
		&& IsValid(TargetASC) && Target != Monster && Target->GetWorld() == Monster->GetWorld()
		&& TargetASC->GetAvatarActor() == Target && !TargetASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead)
		&& !TargetASC->HasMatchingGameplayTag(TBGameplayTags::State_Invulnerable)
		&& FMath::IsFinite(ShotDamage) && ShotDamage > 0.0f
		&& FMath::IsFinite(TraceRange) && TraceRange > 0.0f
		&& FVector::DistSquared(Monster->GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(TraceRange);
}

void UTBGameplayAbility_VampireHunter_Hitscan::ActivateAbility(FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	ATBMonster* Monster = ActorInfo ? Cast<ATBMonster>(ActorInfo->AvatarActor.Get()) : nullptr;
	AActor* Target = GetHunterTarget(Monster);
	const USkeletalMeshComponent* Mesh = IsValid(Monster) ? Monster->GetMesh() : nullptr;
	if (!CanUseAgainst(Monster, Target) || (!MuzzleSocketName.IsNone() && (!Mesh || !Mesh->DoesSocketExist(MuzzleSocketName))))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive())
	{
		return;
	}

	ShotTarget = Target;
	FTimerManager& Timers = Monster->GetWorld()->GetTimerManager();
	Timers.SetTimer(AimLockTimer, this, &ThisClass::LockAim, AimLockTime, false);
	Timers.SetTimer(FireTimer, this, &ThisClass::FireShot, FireTime, false);
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
	if (CVarDebugHunterShot.GetValueOnGameThread() != 0)
	{
		DrawDebugPoint(Monster->GetWorld(), LockedAimLocation.GetValue(), 12.0f, FColor::Cyan, false, 2.0f);
	}
#endif
}

void UTBGameplayAbility_VampireHunter_Hitscan::FireShot()
{
	ATBMonster* Monster = Cast<ATBMonster>(GetAvatarActorFromActorInfo());
	AActor* Target = ShotTarget.Get();
	const USkeletalMeshComponent* Mesh = IsValid(Monster) ? Monster->GetMesh() : nullptr;
	if (!IsActive() || !LockedAimLocation.IsSet() || !CanUseAgainst(Monster, Target)
		|| GetHunterTarget(Monster) != Target
		|| (!MuzzleSocketName.IsNone() && (!Mesh || !Mesh->DoesSocketExist(MuzzleSocketName))))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	FVector TraceStart;
	if (MuzzleSocketName.IsNone())
	{
		FRotator EyeRotation;
		Monster->GetActorEyesViewPoint(TraceStart, EyeRotation);
	}
	else
	{
		TraceStart = Mesh->GetSocketLocation(MuzzleSocketName);
	}

	const FVector AimDirection = (LockedAimLocation.GetValue() - TraceStart).GetSafeNormal();
	if (AimDirection.IsNearlyZero())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	const FVector TraceEnd = TraceStart + AimDirection * TraceRange;
	FHitResult Hit;
	const bool bBlockingHit = Monster->LineTraceByChannel(Hit, TraceStart, TraceEnd, HunterHitscanTraceChannel);
	const bool bHitTarget = bBlockingHit && Hit.GetActor() == Target;

#if ENABLE_DRAW_DEBUG
	if (CVarDebugHunterShot.GetValueOnGameThread() != 0)
	{
		const FColor TraceColor = bHitTarget ? FColor::Green : (bBlockingHit ? FColor::Orange : FColor::Red);
		const FVector DebugEnd = bBlockingHit ? Hit.ImpactPoint : TraceEnd;
		DrawDebugLine(Monster->GetWorld(), TraceStart, DebugEnd, TraceColor, false, 2.0f, 0, 2.0f);
		if (bBlockingHit)
		{
			DrawDebugPoint(Monster->GetWorld(), DebugEnd, 12.0f, TraceColor, false, 2.0f);
		}
	}
#endif

	if (bHitTarget)
	{
		ITBHitInterface::Execute_ReceiveAttackHit(Target, ShotDamage, Monster, Hit);
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
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
	if (bWasCancelled && ActorInfo)
	{
		UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
		if (ASC && ASC->GetAnimatingAbility() == this)
		{
			ASC->CurrentMontageStop();
		}
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
