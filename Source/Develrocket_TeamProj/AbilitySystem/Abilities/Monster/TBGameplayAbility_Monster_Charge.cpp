#include "AbilitySystem/Abilities/Monster/TBGameplayAbility_Monster_Charge.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AIController.h"
#include "AI/TBAIMovementVisibilitySubsystem.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/Monster/TBMonster.h"
#include "Character/Player/TBPlayer.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TBCharacterMovementComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "Interface/TBHitInterface.h"
#include "TBGameplayTags.h"
#include "TimerManager.h"

UTBGameplayEffect_ChargeWindup::UTBGameplayEffect_ChargeWindup()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	auto* TargetTags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("WindupTargetTags"));
	GEComponents.Add(TargetTags);

	FInheritedTagContainer TagChanges;
	TagChanges.AddTag(TBGameplayTags::State_MovementBlocked);
	TargetTags->SetAndApplyTargetTagChanges(TagChanges);
}

UTBGameplayAbility_Monster_Charge::UTBGameplayAbility_Monster_Charge()
{
	SetAssetTags(FGameplayTagContainer(TBGameplayTags::Ability_Combat));
	ActivationOwnedTags.AddTag(TBGameplayTags::State_Combat);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Stunned);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_MovementBlocked);
}

bool UTBGameplayAbility_Monster_Charge::CanUseAgainst(const ATBMonster* Monster, const AActor* Target) const
{
	const UAbilitySystemComponent* TargetASC = IsValid(Target) ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target) : nullptr;
	return IsValid(Monster) && Monster->HasAuthority() && !Monster->IsDead() && IsValid(Target)
		&& Target->IsA<ATBPlayer>() && IsValid(TargetASC) && TargetASC->GetAvatarActor() == Target
		&& !TargetASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead)
		&& Monster->GetWorld() && Target->GetWorld() == Monster->GetWorld()
		&& Monster->GetCharacterMovement() && Monster->GetMesh() && Monster->GetMesh()->GetAnimInstance()
		&& ThirdPersonMontage && ThirdPersonMontage->GetSectionIndex(ChargeSection) > 0
		&& FMath::IsFinite(ChargeSpeed) && ChargeSpeed > 0.f && FMath::IsFinite(MaxDuration) && MaxDuration > 0.f
		&& FMath::IsFinite(MaxDistance) && MaxDistance > 0.f;
}

void UTBGameplayAbility_Monster_Charge::ActivateAbility(FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	ATBMonster* Monster = ActorInfo ? Cast<ATBMonster>(ActorInfo->AvatarActor.Get()) : nullptr;
	AActor* Target = TriggerEventData ? const_cast<AActor*>(TriggerEventData->Target.Get()) : nullptr;
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!IsValid(ASC) || !CanUseAgainst(Monster, Target))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ChargingMonster = Monster;
	ChargeTarget = Target;
	OriginalAnimTickOption = Monster->GetMesh()->VisibilityBasedAnimTickOption;
	bSavedAnimTickOption = true;
	Monster->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	if (AAIController* Controller = Cast<AAIController>(Monster->GetController())) Controller->StopMovement();
	Monster->GetCharacterMovement()->StopMovementImmediately();
	Monster->ConsumeMovementInputVector();

	const FActiveGameplayEffectHandle AppliedWindup = ASC->ApplyGameplayEffectToSelf(
		GetDefault<UTBGameplayEffect_ChargeWindup>(), 1.f, ASC->MakeEffectContext());
	if (!IsActive())
	{
		if (AppliedWindup.IsValid()) ASC->RemoveActiveGameplayEffect(AppliedWindup);
		return;
	}
	WindupEffectHandle = AppliedWindup;
	if (!WindupEffectHandle.IsValid())
	{
		FinishCharge(true);
		return;
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive()) return;
	ASC->CurrentMontageSetNextSectionName(ChargeSection, ChargeSection);
	Monster->GetWorldTimerManager().SetTimer(ChargeTimer, this, &ThisClass::UpdateCharge, 0.05f, true);
}

void UTBGameplayAbility_Monster_Charge::UpdateCharge()
{
	ATBMonster* Monster = ChargingMonster.Get();
	AActor* Target = ChargeTarget.Get();
	UAbilitySystemComponent* ASC = Monster ? Monster->GetAbilitySystemComponent() : nullptr;
	auto* Movement = Monster ? Cast<UTBCharacterMovementComponent>(Monster->GetCharacterMovement()) : nullptr;
	UAnimInstance* Anim = Monster && Monster->GetMesh() ? Monster->GetMesh()->GetAnimInstance() : nullptr;
	if (!IsActive() || !IsValid(Monster) || Monster->IsDead() || !IsValid(Target) || !ASC || !Movement || !Anim
		|| ASC->HasMatchingGameplayTag(TBGameplayTags::State_Stunned) || !Anim->Montage_IsPlaying(ThirdPersonMontage))
	{
		FinishCharge(true);
		return;
	}
	if (!bCharging)
	{
		if (Anim->Montage_GetCurrentSection(ThirdPersonMontage) != ChargeSection) return;
		RemoveWindupEffect();
		if (!IsActive()) return;
		ChargeDirection = (Target->GetActorLocation() - Monster->GetActorLocation()).GetSafeNormal2D();
		if (ChargeDirection.IsNearlyZero()) { FinishCharge(true); return; }
		bCharging = true;
		StartLocation = Monster->GetActorLocation();
		ChargeStartTime = Monster->GetWorld()->GetTimeSeconds();
		OriginalMaxWalkSpeed = Movement->MaxWalkSpeed;
		Movement->MaxWalkSpeed = ChargeSpeed;
		Monster->SetActorRotation(ChargeDirection.Rotation());
	}
	if (Monster->IsMovementBlocked())
	{
		FinishCharge(true);
		return;
	}
	if (Monster->GetWorld()->GetTimeSeconds() - ChargeStartTime >= MaxDuration
		|| FVector::DistSquared2D(StartLocation, Monster->GetActorLocation()) >= FMath::Square(MaxDistance))
	{
		FinishCharge(false);
		return;
	}
	UCapsuleComponent* Capsule = Monster->GetCapsuleComponent();
	if (!Capsule) { FinishCharge(true); return; }
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MonsterCharge), false, Monster);
	FHitResult Hit;
	const FVector Start = Capsule->GetComponentLocation();
	const FVector End = Start + ChargeDirection * FMath::Max(25.f, ChargeSpeed * 0.06f);
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(FMath::Max(1.f, Capsule->GetScaledCapsuleRadius() - 2.f),
		FMath::Max(1.f, Capsule->GetScaledCapsuleHalfHeight() - 2.f));
	if (Monster->GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, Capsule->GetCollisionObjectType(), Shape, Params))
	{
		AActor* Other = Hit.GetActor();
		UAbilitySystemComponent* OtherASC = IsValid(Other) ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Other) : nullptr;
		if (Other && Other->IsA<ATBPlayer>() && OtherASC && OtherASC->GetAvatarActor() == Other
			&& !OtherASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead)
			&& !OtherASC->HasMatchingGameplayTag(TBGameplayTags::State_Invulnerable)
			&& Other->GetClass()->ImplementsInterface(UTBHitInterface::StaticClass()))
		{
			ITBHitInterface::Execute_ReceiveAttackHit(Other, AttackDamage, Monster, Hit);
		}
		FinishCharge(false);
		return;
	}
	if (!Movement->HasChargeInput())
	{
		Movement->SetChargeInputDirection(ChargeDirection);
		if (auto* Visibility = Monster->GetWorld()->GetSubsystem<UTBAIMovementVisibilitySubsystem>())
		{
			Visibility->RestoreMovementTick(*Monster);
		}
	}
}

void UTBGameplayAbility_Monster_Charge::FinishCharge(bool bCancelled)
{
	if (IsActive()) EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bCancelled);
}

void UTBGameplayAbility_Monster_Charge::RemoveWindupEffect()
{
	const FActiveGameplayEffectHandle EffectHandle = WindupEffectHandle;
	WindupEffectHandle.Invalidate();
	if (EffectHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = EffectHandle.GetOwningAbilitySystemComponent())
		{
			ASC->RemoveActiveGameplayEffect(EffectHandle);
		}
	}
}

void UTBGameplayAbility_Monster_Charge::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (ATBMonster* Monster = ChargingMonster.Get())
	{
		Monster->GetWorldTimerManager().ClearTimer(ChargeTimer);
		if (bSavedAnimTickOption && Monster->GetMesh()) Monster->GetMesh()->VisibilityBasedAnimTickOption = OriginalAnimTickOption;
		if (bCharging)
		{
			if (auto* Movement = Cast<UTBCharacterMovementComponent>(Monster->GetCharacterMovement()))
			{
				Movement->SetChargeInputDirection(FVector::ZeroVector);
				Movement->StopMovementImmediately();
				Monster->ConsumeMovementInputVector();
				Movement->MaxWalkSpeed = OriginalMaxWalkSpeed;
			}
		}
	}
	ChargingMonster.Reset();
	ChargeTarget.Reset();
	bSavedAnimTickOption = false;
	bCharging = false;
	RemoveWindupEffect();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
