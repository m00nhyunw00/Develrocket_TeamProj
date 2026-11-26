#include "AbilitySystem/Abilities/Monster/VampireHunter/TBGameplayAbility_VampireHunter_RifleSkills.h"

#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Animation/AnimMontage.h"
#include "Character/Monster/TBMonster.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NativeGameplayTags.h"
#include "TBGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ChargedShotCooldown, "Cooldown.Hunter.ChargedShot");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_NotifyShotCooldown, "Cooldown.Hunter.NotifyShot");

UTBGameplayAbility_VampireHunter_StationaryShot::UTBGameplayAbility_VampireHunter_StationaryShot()
{
	ActivationOwnedTags.AddTag(TBGameplayTags::State_MovementBlocked);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_MovementBlocked);
}

bool UTBGameplayAbility_VampireHunter_StationaryShot::CanUseAgainst(const ATBMonster* Monster, const AActor* Target) const
{
	return (!ThirdPersonMontage || !ThirdPersonMontage->HasRootMotion()) && Super::CanUseAgainst(Monster, Target);
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

	if (const AActor* Target = GetCurrentShotTarget())
	{
		const FVector Direction = Target->GetActorLocation() - Hunter->GetActorLocation();
		Hunter->SetActorRotation(FRotator(0.0, Direction.Rotation().Yaw, 0.0));
	}
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
	GetWorld()->GetTimerManager().SetTimer(ChargeTimer, this, &ThisClass::FinishCharge, ChargeDuration, false);
}

void UTBGameplayAbility_VampireHunter_ChargedShot::FinishCharge()
{
	const bool bFired = FireAtCurrentTarget();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, !bFired);
}

void UTBGameplayAbility_VampireHunter_ChargedShot::EndAbility(FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChargeTimer);
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
	return ThirdPersonMontage && Super::CanUseAgainst(Monster, Target);
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
