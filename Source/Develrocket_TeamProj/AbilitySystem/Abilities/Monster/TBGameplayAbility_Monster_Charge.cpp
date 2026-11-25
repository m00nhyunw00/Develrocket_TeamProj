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
#include "Components/TBMonsterMovementComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "Interface/TBHitInterface.h"
#include "Kismet/GameplayStatics.h"
#include "PCG/CityLayoutCoordinator.h"
#include "TBGameplayTags.h"
#include "TimerManager.h"

namespace
{
constexpr float ChargeUpdateInterval = 0.05f;
constexpr double BlockedChargeGraceSeconds = 0.15;

bool FindChargeWall(const ATBMonster& Monster, const UCapsuleComponent& Capsule,
	const FVector& Start, const FVector& End, FHitResult& OutHit)
{
	const auto* Movement = Monster.GetCharacterMovement();
	const FVector Direction = (End - Start).GetSafeNormal2D();
	if (!Movement || Direction.IsNearlyZero()) return false;

	// Probe above the walkable step height, independently of the Pawn collision response.
	const double CapsuleHalfHeight = Capsule.GetScaledCapsuleHalfHeight();
	const double Lift = FMath::Clamp(0.5 * (Movement->MaxStepHeight + 2.0), 0.0, CapsuleHalfHeight - 1.0);
	const double HalfHeight = CapsuleHalfHeight - Lift;
	const double Radius = FMath::Min(static_cast<double>(Capsule.GetScaledCapsuleRadius()), HalfHeight);
	const FVector Offset(0.0, 0.0, Lift);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MonsterChargeWall), true, &Monster);
	const FCollisionObjectQueryParams Objects(ECC_TO_BITFIELD(ECC_WorldStatic) | ECC_TO_BITFIELD(ECC_WorldDynamic));
	TArray<FHitResult> Hits;
	Monster.GetWorld()->SweepMultiByObjectType(Hits, Start + Offset, End + Offset, FQuat::Identity,
		Objects, FCollisionShape::MakeCapsule(Radius, HalfHeight), Params);

	for (const FHitResult& Hit : Hits)
	{
		const UPrimitiveComponent* Component = Hit.GetComponent();
		if (!Component || ACityLayoutCoordinator::IsChargeBreakableProp(Component)) continue;
		if (Component->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block
			&& Component->GetCollisionResponseToChannel(ECC_Visibility) != ECR_Block
			&& Component->GetCollisionResponseToChannel(ECC_WorldStatic) != ECR_Block) continue;
		if (Hit.ImpactNormal.Z >= Movement->GetWalkableFloorZ() || FVector::DotProduct(Hit.ImpactNormal, Direction) >= -0.1) continue;

		OutHit = Hit;
		return true;
	}
	return false;
}
}

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
	const bool bCanCharge = IsValid(Monster) && Monster->HasAuthority() && !Monster->IsDead() && IsValid(Target)
		&& Target->IsA<ATBPlayer>() && IsValid(TargetASC) && TargetASC->GetAvatarActor() == Target
		&& !TargetASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead)
		&& Monster->GetWorld() && Target->GetWorld() == Monster->GetWorld()
		&& Monster->GetCharacterMovement() && Monster->GetMesh() && Monster->GetMesh()->GetAnimInstance()
		&& ThirdPersonMontage && ThirdPersonMontage->GetSectionIndex(ChargeSection) > 0
		&& FMath::IsFinite(ChargeSpeed) && ChargeSpeed > 0.f && FMath::IsFinite(MaxDuration) && MaxDuration > 0.f
		&& FMath::IsFinite(MaxDistance) && MaxDistance > 0.f;
	if (!bCanCharge) return false;

	const AAIController* Controller = Cast<AAIController>(Monster->GetController());
	if (!Controller || !Controller->LineOfSightTo(Target)) return false;

	const FVector Start = Monster->GetActorLocation();
	FVector End = Target->GetActorLocation();
	End.Z = Start.Z;
	FHitResult Wall;
	return !FindChargeWall(*Monster, *Monster->GetCapsuleComponent(), Start, End, Wall);
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
	Monster->GetWorldTimerManager().SetTimer(ChargeTimer, this, &ThisClass::UpdateCharge, ChargeUpdateInterval, true);
}

void UTBGameplayAbility_Monster_Charge::HandleChargeMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity)
{
	ACityLayoutCoordinator* City = CityLayout.Get();
	ATBMonster* Monster = ChargingMonster.Get();
	const UCapsuleComponent* Capsule = Monster ? Monster->GetCapsuleComponent() : nullptr;
	if (!bCharging || !Capsule) return;

	const FVector CurrentLocation = Capsule->GetComponentLocation();
	FVector PathEnd = CurrentLocation;
	FHitResult WallHit;
	const bool bHitWall = FindChargeWall(*Monster, *Capsule, OldLocation, CurrentLocation, WallHit);
	if (bHitWall)
	{
		const FVector Travel = CurrentLocation - OldLocation;
		const double SafeFraction = FMath::Max(0.0, WallHit.Time - 1.0 / Travel.Size());
		PathEnd = OldLocation + Travel * SafeFraction;
		Monster->SetActorLocation(Monster->GetActorLocation() + PathEnd - CurrentLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}
	const double Radius = Capsule->GetScaledCapsuleRadius();
	const FVector Extent(Radius, Radius, Capsule->GetScaledCapsuleHalfHeight());
	if (City && City->BreakChargePropsAlongPath(OldLocation, PathEnd, Extent))
	{
		LastForwardProgressTime = City->GetWorld()->GetTimeSeconds();
	}
	if (bHitWall) FinishCharge(false);
}

void UTBGameplayAbility_Monster_Charge::UpdateCharge()
{
	ATBMonster* Monster = ChargingMonster.Get();
	AActor* Target = ChargeTarget.Get();
	UAbilitySystemComponent* ASC = Monster ? Monster->GetAbilitySystemComponent() : nullptr;
	auto* Movement = Monster ? Monster->GetMonsterMovement() : nullptr;
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
		bCharging = true;
		StartLocation = Monster->GetActorLocation();
		ChargeStartTime = Monster->GetWorld()->GetTimeSeconds();
		LastSampleLocation = StartLocation;
		LastForwardProgressTime = ChargeStartTime;
		OriginalMaxWalkSpeed = Movement->MaxWalkSpeed;
		Movement->MaxWalkSpeed = ChargeSpeed;
		CityLayout = Cast<ACityLayoutCoordinator>(UGameplayStatics::GetActorOfClass(Monster, ACityLayoutCoordinator::StaticClass()));
		Monster->OnCharacterMovementUpdated.AddUniqueDynamic(this, &ThisClass::HandleChargeMovementUpdated);
	}
	if (Monster->IsMovementBlocked())
	{
		FinishCharge(true);
		return;
	}
	const FVector Location = Monster->GetActorLocation();
	const double Now = Monster->GetWorld()->GetTimeSeconds();
	if (Now - ChargeStartTime >= MaxDuration || FVector::DistSquared2D(StartLocation, Location) >= FMath::Square(MaxDistance))
	{
		FinishCharge(false);
		return;
	}
	UCapsuleComponent* Capsule = Monster->GetCapsuleComponent();
	if (!Capsule) { FinishCharge(true); return; }
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MonsterCharge), false, Monster);
	FHitResult Hit;
	const FVector Start = Capsule->GetComponentLocation();
	const FVector Forward = Monster->GetActorForwardVector().GetSafeNormal2D();
	const FVector End = Start + Forward * FMath::Max(25.f, ChargeSpeed * 0.06f);
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(FMath::Max(1.f, Capsule->GetScaledCapsuleRadius() - 2.f),
		FMath::Max(1.f, Capsule->GetScaledCapsuleHalfHeight() - 2.f));
	const bool bObstacleAhead = Monster->GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity,
		Capsule->GetCollisionObjectType(), Shape, Params, FCollisionResponseParams(Capsule->GetCollisionResponseToChannels()));
	AActor* Other = Hit.GetActor();
	FHitResult WallHit;
	if (bObstacleAhead && IsValid(Other) && Other->IsA<ATBPlayer>() && !FindChargeWall(*Monster, *Capsule, Start, Hit.Location, WallHit))
	{
		UAbilitySystemComponent* OtherASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Other);
		if (OtherASC && OtherASC->GetAvatarActor() == Other
			&& !OtherASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead)
			&& !OtherASC->HasMatchingGameplayTag(TBGameplayTags::State_Invulnerable)
			&& Other->GetClass()->ImplementsInterface(UTBHitInterface::StaticClass()))
		{
			ITBHitInterface::Execute_ReceiveAttackHit(Other, AttackDamage, Monster, Hit);
		}
		FinishCharge(false);
		return;
	}
	const double ForwardProgress = FVector::DotProduct(Location - LastSampleLocation, Forward);
	LastSampleLocation = Location;
	if (!bObstacleAhead || ForwardProgress >= ChargeSpeed * ChargeUpdateInterval * 0.1)
	{
		LastForwardProgressTime = Now;
	}
	if (Now - LastForwardProgressTime >= BlockedChargeGraceSeconds)
	{
		FinishCharge(false);
		return;
	}
	if (!Movement->HasChargeInput())
	{
		Movement->SetChargeMovementActive(true);
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
			Monster->OnCharacterMovementUpdated.RemoveDynamic(this, &ThisClass::HandleChargeMovementUpdated);
			if (auto* Movement = Monster->GetMonsterMovement())
			{
				Movement->SetChargeMovementActive(false);
				Movement->StopMovementImmediately();
				Monster->ConsumeMovementInputVector();
				Movement->MaxWalkSpeed = OriginalMaxWalkSpeed;
			}
		}
	}
	ChargingMonster.Reset();
	ChargeTarget.Reset();
	CityLayout.Reset();
	bSavedAnimTickOption = false;
	bCharging = false;
	RemoveWindupEffect();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
