#include "AbilitySystem/Abilities/Monster/TBGameplayAbility_Monster_Boom.h"
#include "AIController.h"

#include "AbilitySystemComponent.h"
#include "AI/TBAIMovementVisibilitySubsystem.h"
#include "AIController.h"
#include "Animation/AnimMontage.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/Human/TBCitizen.h"
#include "Character/Monster/TBMonster.h"
#include "Character/Player/TBPlayer.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TBMonsterMovementComponent.h"
#include "Components/TBSpawnManagerComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Interface/TBHitInterface.h"
#include "TBGameplayTags.h"

namespace
{
bool IsBoomVictim(const AActor* Actor)
{
	return IsValid(Actor) && (Actor->IsA<ATBPlayer>() || Actor->IsA<ATBMonster>() || Actor->IsA<ATBCitizen>());
}
}

UTBGameplayAbility_Monster_Boom::UTBGameplayAbility_Monster_Boom()
{
	SetAssetTags(FGameplayTagContainer(TBGameplayTags::Ability_Combat_MonsterBoom));
	ActivationOwnedTags.AddTag(TBGameplayTags::State_Combat);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Stunned);
}

bool UTBGameplayAbility_Monster_Boom::CanUseAgainst(const ATBMonster* Monster, const AActor* Target) const
{
	const ATBPlayer* Player = Cast<ATBPlayer>(Target);
	const UAbilitySystemComponent* TargetASC = IsValid(Player) ? Player->GetAbilitySystemComponent() : nullptr;
	const bool bCanJump = IsValid(Monster) && Monster->HasAuthority() && !Monster->IsDead() && IsValid(TargetASC)
		&& TargetASC->GetAvatarActor() == Player && !TargetASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead)
		&& Monster->GetWorld() == Player->GetWorld() && Monster->GetMesh()->GetAnimInstance()
		&& ThirdPersonMontage && FMath::IsFinite(ExplosionRadius) && ExplosionRadius > 0.f
		&& FMath::IsFinite(ExplosionDamage) && ExplosionDamage > 0.f;
	if (!bCanJump) return false;

	const AAIController* Controller = Cast<AAIController>(Monster->GetController());
	return Controller && Controller->LineOfSightTo(Player)
		&& (Monster->GetMonsterBehavior() != ETBMonsterBehavior::SpideEye
			|| FVector::DistSquared2D(Monster->GetActorLocation(), Player->GetActorLocation()) >= FMath::Square(230.0));
}

void UTBGameplayAbility_Monster_Boom::ActivateAbility(FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	ATBMonster* Monster = ActorInfo ? Cast<ATBMonster>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!CanUseAgainst(Monster, TriggerEventData ? TriggerEventData->Target.Get() : nullptr))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bDetonated = false;
	bJumped = false;
	BlockingImpact.Reset();
	USkeletalMeshComponent* Mesh = Monster->GetMesh();
	OriginalAnimTickOption = Mesh->VisibilityBasedAnimTickOption;
	bSavedAnimTickOption = true;
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UTBGameplayAbility_Monster_Boom::JumpForward(const UAnimSequenceBase* Animation)
{
	ATBMonster* Monster = Cast<ATBMonster>(GetAvatarActorFromActorInfo());
	if (!IsActive() || bJumped || bDetonated || Animation != ThirdPersonMontage || !IsValid(Monster)
		|| !FMath::IsFinite(JumpTravelTime) || JumpTravelTime < 0.05f)
	{
		return;
	}

	const AAIController* Controller = Cast<AAIController>(Monster->GetController());
	const UBlackboardComponent* Blackboard = Controller ? Controller->GetBlackboardComponent() : nullptr;
	const auto* Target = Blackboard ? Cast<ATBPlayer>(Blackboard->GetValueAsObject(TEXT("TargetActor"))) : nullptr;
	auto* Movement = Monster->GetMonsterMovement();
	if (!CanUseAgainst(Monster, Target) || !Movement || Movement->MovementMode == MOVE_None)
	{
		return;
	}

	FVector Offset = Target->GetActorLocation() - Monster->GetActorLocation();
	Offset.Z += Monster->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
		- Target->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const double HorizontalSpeed = Offset.Size2D() / JumpTravelTime;
	FVector Velocity = Monster->GetActorForwardVector().GetSafeNormal2D() * HorizontalSpeed;
	Velocity.Z = Offset.Z / JumpTravelTime - 0.5 * Movement->GetGravityZ() * JumpTravelTime;

	bJumped = true;
	Monster->OnCharacterMovementUpdated.AddUniqueDynamic(this, &ThisClass::HandleMovementUpdated);
	Monster->LandedDelegate.AddUniqueDynamic(this, &ThisClass::RecordImpact);
	Movement->OnBoomJumpImpact.AddUObject(this, &ThisClass::RecordImpact);
	Movement->SetBoomJumpActive(true);
	if (auto* Visibility = Monster->GetWorld()->GetSubsystem<UTBAIMovementVisibilitySubsystem>())
	{
		Visibility->RestoreMovementTick(*Monster);
	}
	Monster->ConsumeMovementInputVector();
	Monster->LaunchCharacter(Velocity, true, true);
}

void UTBGameplayAbility_Monster_Boom::HandleActionMontageCompleted()
{
	if (!bJumped) Super::HandleActionMontageCompleted();
}

void UTBGameplayAbility_Monster_Boom::RecordImpact(const FHitResult& Hit)
{
	if (bJumped && !bDetonated && !BlockingImpact.IsSet())
	{
		BlockingImpact = Hit;
	}
}

void UTBGameplayAbility_Monster_Boom::HandleMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity)
{
	ATBMonster* Monster = Cast<ATBMonster>(GetAvatarActorFromActorInfo());
	if (!IsActive() || !bJumped || bDetonated || !IsValid(Monster)
		|| !Monster->HasAuthority() || Monster->IsDead() || !Monster->GetWorld())
	{
		return;
	}

	const UCapsuleComponent* Capsule = Monster->GetCapsuleComponent();
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(
		Capsule->GetScaledCapsuleRadius() + 1.f, Capsule->GetScaledCapsuleHalfHeight() + 1.f);
	// Check pawn contacts along the jump before resolving the first blocking impact.
	const FVector SweepEnd = BlockingImpact.IsSet() ? FVector(BlockingImpact->Location) : Capsule->GetComponentLocation();
	TArray<FHitResult> Contacts;
	Monster->GetWorld()->SweepMultiByObjectType(Contacts, OldLocation, SweepEnd,
		Capsule->GetComponentQuat(), FCollisionObjectQueryParams(ECC_Pawn), Shape,
		FCollisionQueryParams(SCENE_QUERY_STAT(MonsterBoomContact), false, Monster));
	for (const FHitResult& Contact : Contacts)
	{
		const auto* Other = Cast<ATBCharacterBase>(Contact.GetActor());
		const UAbilitySystemComponent* ASC = IsBoomVictim(Other) ? Other->GetAbilitySystemComponent() : nullptr;
		if (IsValid(ASC) && ASC->GetAvatarActor() == Other && !ASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead))
		{
			Detonate(*Monster, Contact);
			return;
		}
	}

	if (BlockingImpact.IsSet())
	{
		Detonate(*Monster, BlockingImpact.GetValue());
	}
}

namespace
{
struct FSmallEyeSizeVariant
{
	float Scale;
	float WalkSpeedMultiplier;
};

constexpr FSmallEyeSizeVariant SmallEyeSizeVariants[] = {
	{1.0f, 1.0f},
	{0.7f, 1.15f},
	{0.3f, 1.3f},
};

bool IsSmallEyeClass(const UClass* ActorClass)
{
	static const FName SmallEyeClassPath(
		TEXT("/Game/_Game/Blueprints/Characters/Monster/SpideEye/BP_SmallEye.BP_SmallEye_C"));
	for (const UClass* Class = ActorClass; Class; Class = Class->GetSuperClass())
	{
		if (FName(*Class->GetPathName()) == SmallEyeClassPath) return true;
	}
	return false;
}
}

void UTBGameplayAbility_Monster_Boom::Detonate(ATBMonster& SourceMonster, const FHitResult& Impact)
{
	ATBMonster* Monster = &SourceMonster;
	bDetonated = true;
	UWorld* World = Monster->GetWorld();
	UTBSpawnManagerComponent* SpawnManager = UTBSpawnManagerComponent::FindForActor(Monster);
	const FVector Origin = Impact.bStartPenetrating ? Impact.Location : Impact.ImpactPoint;
	const FVector SurfaceNormal = Impact.ImpactNormal.GetSafeNormal();
	const FTransform SpawnTransform(Monster->GetActorRotation(), Origin);
	TArray<FTBMonsterBoomSpawnEntry> Entries = SpawnEntries;
	if (Entries.IsEmpty())
	{
		for (const TSubclassOf<AActor>& ActorClass : SpawnActorClasses)
		{
			Entries.Add({ActorClass, 1});
		}
	}

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, FCollisionObjectQueryParams::AllDynamicObjects,
		FCollisionShape::MakeSphere(ExplosionRadius), FCollisionQueryParams(SCENE_QUERY_STAT(MonsterBoom), false, Monster));

	TSet<AActor*> HitActors;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Victim = Overlap.GetActor();
		if (!IsBoomVictim(Victim) || HitActors.Contains(Victim))
		{
			continue;
		}

		HitActors.Add(Victim);
		ITBHitInterface::Execute_ReceiveAttackHit(Victim, ExplosionDamage, Monster, FHitResult());
	}

	if (IsValid(Monster))
	{
		if (ExplosionEffect)
		{
			Monster->FlushNetDormancy();
			Monster->MulticastPlayBoomEffect(ExplosionEffect, Origin);
		}
		if (IsActive()) EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		if (IsValid(Monster) && !UTBSpawnManagerComponent::ReturnActorToPool(Monster))
		{
			Monster->Destroy();
		}
	}

	SpawnExplosionActors(*World, SpawnManager, SpawnTransform, SurfaceNormal, Entries);
}

void UTBGameplayAbility_Monster_Boom::SpawnExplosionActors(UWorld& World, UTBSpawnManagerComponent* SpawnManager,
	const FTransform& ImpactTransform, const FVector& SurfaceNormal, const TArray<FTBMonsterBoomSpawnEntry>& Entries) const
{
	int64 SpawnCount = 0;
	double Spacing = 100.0;
	for (const FTBMonsterBoomSpawnEntry& Entry : Entries)
	{
		if (!Entry.ActorClass || Entry.Count <= 0) continue;
		SpawnCount += Entry.Count;
		if (const ACharacter* Character = Cast<ACharacter>(Entry.ActorClass.GetDefaultObject()))
		{
			Spacing = FMath::Max(Spacing, 2.0 * Character->GetCapsuleComponent()->GetScaledCapsuleRadius() + 10.0);
		}
	}

	const FVector AwayFromWall = SurfaceNormal.GetSafeNormal2D();
	const bool bWall = FMath::Abs(SurfaceNormal.Z) < 0.5 && !AwayFromWall.IsNearlyZero();
	const double SweepAngle = bWall ? PI : 2.0 * PI;
	const double AngleStep = SpawnCount > 1 ? SweepAngle / (bWall ? SpawnCount - 1 : SpawnCount) : 0.0;
	const double SpawnRadius = SpawnCount > 1 ? Spacing / (2.0 * FMath::Sin(AngleStep * 0.5)) : 0.0;
	const double StartAngle = bWall ? FMath::Atan2(AwayFromWall.Y, AwayFromWall.X) - PI * 0.5 : 0.0;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	int64 SpawnIndex = 0;
	for (const FTBMonsterBoomSpawnEntry& Entry : Entries)
	{
		if (!Entry.ActorClass || Entry.Count <= 0) continue;
		const ACharacter* Character = Cast<ACharacter>(Entry.ActorClass.GetDefaultObject());
		const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
		const bool bSmallEye = IsSmallEyeClass(Entry.ActorClass.Get());
		for (int32 Index = 0; Index < Entry.Count; ++Index, ++SpawnIndex)
		{
			const FSmallEyeSizeVariant Variant = bSmallEye
				? SmallEyeSizeVariants[FMath::RandHelper(UE_ARRAY_COUNT(SmallEyeSizeVariants))]
				: FSmallEyeSizeVariant{1.0f, 1.0f};
			const float Scale = Variant.Scale;
			const double Radius = Capsule ? Capsule->GetScaledCapsuleRadius() * Scale : 0.0;
			const double HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() * Scale : 0.0;
			const double SurfaceClearance = 2.0 + Radius + (HalfHeight - Radius) * FMath::Abs(SurfaceNormal.Z);
			const double Angle = StartAngle + SpawnIndex * AngleStep;
			FVector Offset(FMath::Cos(Angle) * SpawnRadius, FMath::Sin(Angle) * SpawnRadius, 0.0);
			Offset += SurfaceNormal * FMath::Max(0.0, SurfaceClearance - FVector::DotProduct(Offset, SurfaceNormal));
			FTransform SpawnTransform = ImpactTransform;
			SpawnTransform.AddToTranslation(Offset);
			if (!bSmallEye)
			{
				World.SpawnActor<AActor>(Entry.ActorClass, SpawnTransform, Params);
				continue;
			}

			if (!IsValid(SpawnManager) || !Capsule) continue;
			const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
			const FCollisionResponseParams Responses(Capsule->GetCollisionResponseToChannels());
			const FCollisionQueryParams Query(SCENE_QUERY_STAT(SmallEyeSpawn));
			for (int32 Attempt = 0; Attempt < 7; ++Attempt)
			{
				FVector Position = SpawnTransform.GetLocation();
				if (Attempt > 0)
				{
					const double SearchAngle = Angle + (Attempt - 1) * PI / 3.0;
					Position += FVector(FMath::Cos(SearchAngle), FMath::Sin(SearchAngle), 0.0) * Spacing;
				}
				const double Clearance = FVector::DotProduct(Position - ImpactTransform.GetLocation(), SurfaceNormal);
				Position += SurfaceNormal * FMath::Max(0.0, SurfaceClearance - Clearance);
				if (World.OverlapBlockingTestByChannel(Position, FQuat::Identity, Capsule->GetCollisionObjectType(), Shape, Query, Responses))
				{
					continue;
				}
				FTransform PooledTransform = SpawnTransform;
				PooledTransform.SetLocation(Position);
				SpawnManager->SpawnSummonedMonster(Entry.ActorClass, PooledTransform, Scale,
					Variant.WalkSpeedMultiplier, SmallEyePoolCapacity);
				break;
			}
		}
	}
}

void UTBGameplayAbility_Monster_Boom::EndAbility(FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (ATBMonster* Monster = Cast<ATBMonster>(GetAvatarActorFromActorInfo()))
	{
		if (bSavedAnimTickOption)
		{
			Monster->GetMesh()->VisibilityBasedAnimTickOption = OriginalAnimTickOption;
		}
		if (bJumped)
		{
			Monster->OnCharacterMovementUpdated.RemoveDynamic(this, &ThisClass::HandleMovementUpdated);
			Monster->LandedDelegate.RemoveDynamic(this, &ThisClass::RecordImpact);
			if (auto* Movement = Monster->GetMonsterMovement())
			{
				Movement->OnBoomJumpImpact.RemoveAll(this);
				Movement->SetBoomJumpActive(false);
			}
		}
	}
	bSavedAnimTickOption = false;
	bJumped = false;
	BlockingImpact.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
