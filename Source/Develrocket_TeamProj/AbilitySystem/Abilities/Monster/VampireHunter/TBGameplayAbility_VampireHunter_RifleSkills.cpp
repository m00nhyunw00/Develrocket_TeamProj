#include "AbilitySystem/Abilities/Monster/VampireHunter/TBGameplayAbility_VampireHunter_RifleSkills.h"

#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Animation/AnimMontage.h"
#include "Animation/Monster/TBAnimNotify_HunterRifleFire.h"
#include "Character/Monster/TBMonster.h"
#include "Components/TBHunterCombatComponent.h"
#include "Components/TBRotationComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "NativeGameplayTags.h"
#include "TBGameplayTags.h"
#include "UObject/ConstructorHelpers.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ChargedShotCooldown, "Cooldown.Hunter.ChargedShot");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_NotifyShotCooldown, "Cooldown.Hunter.NotifyShot");

#if ENABLE_DRAW_DEBUG
namespace
{
constexpr float ChargeDebugInterval = 0.05f;
}
#endif

UTBGameplayAbility_VampireHunter_StationaryShot::UTBGameplayAbility_VampireHunter_StationaryShot()
{
	static const ConstructorHelpers::FClassFinder<UGameplayEffect> DefaultCooldownEffect(
		TEXT("/Game/_Game/Blueprints/AbilitySystem/Monster/GE_Monster_SkillCooldown"));
	SkillCooldownEffectClass = DefaultCooldownEffect.Class;
	ActivationOwnedTags.AddTag(TBGameplayTags::State_MovementBlocked);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_MovementBlocked);
}

bool UTBGameplayAbility_VampireHunter_StationaryShot::CanUseAgainst(const ATBMonster* Monster, const AActor* Target) const
{
	if (!Super::CanUseAgainst(Monster, Target))
	{
		return false;
	}

	const UAnimMontage* Montage = ResolveShotMontage(*Monster);
	return !Montage || (!Montage->HasRootMotion() && FMath::IsFinite(Montage->RateScale) && Montage->RateScale > 0.0f
		&& FMath::IsFinite(Montage->GetPlayLength()) && Montage->GetPlayLength() > 0.0f);
}

void UTBGameplayAbility_VampireHunter_StationaryShot::OnActionCommitted()
{
	Super::OnActionCommitted();
	ATBMonster* Hunter = GetHunter();
	AAIController* Controller = Cast<AAIController>(Hunter->GetController());
	if (Controller)
	{
		Controller->StopMovement();
	}
	Hunter->GetCharacterMovement()->StopMovementImmediately();

	if (UTBRotationComponent* Rotation = Hunter->GetAttackRotationComponent())
	{
		ShotRotation = Rotation;
		Rotation->SetAbilityFacingTarget(GetCurrentShotTarget(false));
		Rotation->ToggleShouldRotate(false);
	}
}

void UTBGameplayAbility_VampireHunter_StationaryShot::SetTrackingTarget(bool bTrack)
{
	if (UTBRotationComponent* Rotation = ShotRotation.Get())
	{
		Rotation->ToggleShouldRotate(bTrack);
	}
}

void UTBGameplayAbility_VampireHunter_StationaryShot::EndAbility(FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UTBRotationComponent* Rotation = ShotRotation.Get())
	{
		Rotation->ToggleShouldRotate(false);
		Rotation->SetAbilityFacingTarget(nullptr);
	}
	ShotRotation.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UTBGameplayAbility_VampireHunter_StationaryShot::FireAtCurrentTarget() const
{
	AActor* Target = GetCurrentShotTarget(false);
	return Target && TraceShot(*GetHunter(), *Target, Target->GetActorLocation());
}

UTBGameplayAbility_VampireHunter_ChargedShot::UTBGameplayAbility_VampireHunter_ChargedShot()
{
	ShotDamage = 35.0f;
	SkillCooldownDuration = 10.0f;
}

const FGameplayTagContainer& UTBGameplayAbility_VampireHunter_ChargedShot::GetConfiguredSkillCooldownTags() const
{
	static const FGameplayTagContainer Tags(TAG_ChargedShotCooldown);
	return Tags;
}

bool UTBGameplayAbility_VampireHunter_ChargedShot::CanUseAgainst(const ATBMonster* Monster, const AActor* Target) const
{
	return FMath::IsFinite(ChargeDuration) && ChargeDuration > 0.f && Super::CanUseAgainst(Monster, Target);
}

void UTBGameplayAbility_VampireHunter_ChargedShot::StartShotSequence()
{
	SetTrackingTarget(true);
	GetHunter()->GetHunterCombatComponent()->BeginRifleChargeVisual(
		GetCurrentShotTarget(false), MuzzleSocketName, TraceRange, ChargeDuration);
	FTimerManager& Timers = GetWorld()->GetTimerManager();
	Timers.SetTimer(ChargeTimer, this, &ThisClass::FinishCharge, ChargeDuration, false);
#if ENABLE_DRAW_DEBUG
	UpdateChargeDebugRay();
	Timers.SetTimer(ChargeDebugTimer, this, &ThisClass::UpdateChargeDebugRay, ChargeDebugInterval, true);
#endif
}

void UTBGameplayAbility_VampireHunter_ChargedShot::UpdateChargeDebugRay()
{
#if ENABLE_DRAW_DEBUG
	if (!IsShotDebugEnabled())
	{
		return;
	}

	const AActor* Target = GetCurrentShotTarget(false);
	const ATBMonster* Hunter = GetHunter();
	FVector Start;
	if (Target && Hunter && TryGetShotOrigin(*Hunter, Start))
	{
		const FVector End = Start + (Target->GetActorLocation() - Start).GetClampedToMaxSize(TraceRange);
		DrawDebugLine(Hunter->GetWorld(), Start, End, FColor::Yellow, false, ChargeDebugInterval, 0, 2.0f);
	}
#endif
}

void UTBGameplayAbility_VampireHunter_ChargedShot::FinishCharge()
{
	SetTrackingTarget(false);
	if (ATBMonster* Hunter = GetHunter())
	{
		Hunter->GetHunterCombatComponent()->EndRifleChargeVisual();
	}
	GetWorld()->GetTimerManager().ClearTimer(ChargeDebugTimer);
	if (!GetCurrentShotTarget(false))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	if (ThirdPersonMontage)
	{
		PlayThirdPersonActionMontage(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo);
	}
	if (!IsActive())
	{
		return;
	}

	const bool bFired = FireAtCurrentTarget();
	if (IsActive() && (!bFired || !ThirdPersonMontage))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, !bFired);
	}
}

void UTBGameplayAbility_VampireHunter_ChargedShot::OnShotTraced(ATBMonster& Monster, const FVector& Start, const FVector& End) const
{
	Monster.GetHunterCombatComponent()->MulticastPlayChargedShotFlash(Start, End);
}

void UTBGameplayAbility_VampireHunter_ChargedShot::EndAbility(FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (ATBMonster* Hunter = GetHunter())
	{
		Hunter->GetHunterCombatComponent()->EndRifleChargeVisual();
	}
	if (UWorld* World = GetWorld())
	{
		FTimerManager& Timers = World->GetTimerManager();
		Timers.ClearTimer(ChargeTimer);
		Timers.ClearTimer(ChargeDebugTimer);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

UTBGameplayAbility_VampireHunter_NotifyShot::UTBGameplayAbility_VampireHunter_NotifyShot()
{
	ShotDamage = 12.0f;
	SkillCooldownDuration = 8.0f;
}

const FGameplayTagContainer& UTBGameplayAbility_VampireHunter_NotifyShot::GetConfiguredSkillCooldownTags() const
{
	static const FGameplayTagContainer Tags(TAG_NotifyShotCooldown);
	return Tags;
}

bool UTBGameplayAbility_VampireHunter_NotifyShot::CanUseAgainst(const ATBMonster* Monster, const AActor* Target) const
{
	if (!Super::CanUseAgainst(Monster, Target))
	{
		return false;
	}

	const UAnimMontage* Montage = ResolveShotMontage(*Monster);
	return Montage && Montage->Notifies.ContainsByPredicate([](const FAnimNotifyEvent& Event)
	{
		return IsValid(Event.Notify.Get()) && Event.Notify->IsA<UTBAnimNotify_HunterRifleFire>()
			&& Event.bTriggerOnDedicatedServer;
	});
}

UAnimMontage* UTBGameplayAbility_VampireHunter_NotifyShot::GetPhaseShotMontage(const ATBMonster& Monster) const
{
	return Monster.GetHunterCombatComponent()->GetRifleNotifyShotMontage();
}

void UTBGameplayAbility_VampireHunter_NotifyShot::OnActionCommitted()
{
	Super::OnActionCommitted();
	if (USkeletalMeshComponent* Mesh = GetHunter()->GetMesh())
	{
		// Dedicated servers must advance the montage and refresh the muzzle socket.
		NotifyMesh = Mesh;
		PreviousVisibilityTickOption = static_cast<uint8>(Mesh->VisibilityBasedAnimTickOption);
		Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}
}

void UTBGameplayAbility_VampireHunter_NotifyShot::FireFromMontageNotify(const UAnimSequenceBase* Animation) const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (Animation && Animation == ThirdPersonMontage.Get() && ASC
		&& ASC->GetCurrentMontage() == ThirdPersonMontage.Get() && ASC->GetAnimatingAbility() == this)
	{
		FireAtCurrentTarget();
	}
}

void UTBGameplayAbility_VampireHunter_NotifyShot::EndAbility(FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (USkeletalMeshComponent* Mesh = NotifyMesh.Get())
	{
		Mesh->VisibilityBasedAnimTickOption = static_cast<EVisibilityBasedAnimTickOption>(PreviousVisibilityTickOption);
	}
	NotifyMesh.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
