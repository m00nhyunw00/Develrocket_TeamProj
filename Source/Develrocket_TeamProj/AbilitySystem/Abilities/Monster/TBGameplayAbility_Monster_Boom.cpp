#include "AbilitySystem/Abilities/Monster/TBGameplayAbility_Monster_Boom.h"

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
	return IsValid(Monster) && Monster->HasAuthority() && !Monster->IsDead() && IsValid(TargetASC)
		&& TargetASC->GetAvatarActor() == Player && !TargetASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead)
		&& Monster->GetWorld() == Player->GetWorld() && Monster->GetMesh()->GetAnimInstance()
		&& ThirdPersonMontage && FMath::IsFinite(ExplosionRadius) && ExplosionRadius > 0.f
		&& FMath::IsFinite(ExplosionDamage) && ExplosionDamage > 0.f;
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
	bLanded = false;
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
	Monster->LandedDelegate.AddUniqueDynamic(this, &ThisClass::HandleLanded);
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

void UTBGameplayAbility_Monster_Boom::HandleLanded(const FHitResult& Hit)
{
	bLanded = true;
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
	TArray<FHitResult> Contacts;
	Monster->GetWorld()->SweepMultiByObjectType(Contacts, OldLocation, Capsule->GetComponentLocation(),
		Capsule->GetComponentQuat(), FCollisionObjectQueryParams(ECC_Pawn), Shape,
		FCollisionQueryParams(SCENE_QUERY_STAT(MonsterBoomContact), false, Monster));
	for (const FHitResult& Contact : Contacts)
	{
		const auto* Other = Cast<ATBCharacterBase>(Contact.GetActor());
		const UAbilitySystemComponent* ASC = IsBoomVictim(Other) ? Other->GetAbilitySystemComponent() : nullptr;
		if (IsValid(ASC) && ASC->GetAvatarActor() == Other && !ASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead))
		{
			Detonate(*Monster);
			return;
		}
	}

	if (bLanded)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UTBGameplayAbility_Monster_Boom::Detonate(ATBMonster& SourceMonster)
{
	ATBMonster* Monster = &SourceMonster;
	bDetonated = true;
	UWorld* World = Monster->GetWorld();
	const FVector Origin = Monster->GetActorLocation();
	const FTransform SpawnTransform(Monster->GetActorRotation(), Origin);
	const auto ClassesToSpawn = SpawnActorClasses;

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

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (const TSubclassOf<AActor>& ActorClass : ClassesToSpawn)
	{
		if (ActorClass) World->SpawnActor<AActor>(ActorClass, SpawnTransform, Params);
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
			Monster->LandedDelegate.RemoveDynamic(this, &ThisClass::HandleLanded);
			if (auto* Movement = Monster->GetMonsterMovement())
			{
				Movement->SetBoomJumpActive(false);
			}
		}
	}
	bSavedAnimTickOption = false;
	bJumped = false;
	bLanded = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
