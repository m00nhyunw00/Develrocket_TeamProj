#include "AbilitySystem/Abilities/Monster/VampireHunter/TBGameplayAbility_VampireHunter_Hitscan.h"

#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/Monster/TBMonster.h"
#include "Components/SkeletalMeshComponent.h"
#include "Interface/TBHitInterface.h"
#include "TBGameplayTags.h"

namespace
{
const FName TargetActorKey(TEXT("TargetActor"));
constexpr ECollisionChannel HitscanTraceChannel = ECC_GameTraceChannel2;
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
	const AAIController* Controller = IsValid(Monster) ? Cast<AAIController>(Monster->GetController()) : nullptr;
	const UBlackboardComponent* Blackboard = Controller ? Controller->GetBlackboardComponent() : nullptr;
	AActor* Target = Blackboard ? Cast<AActor>(Blackboard->GetValueAsObject(TargetActorKey)) : nullptr;
	const USkeletalMeshComponent* Mesh = IsValid(Monster) ? Monster->GetMesh() : nullptr;
	if (!CanUseAgainst(Monster, Target) || (!MuzzleSocketName.IsNone() && (!Mesh || !Mesh->DoesSocketExist(MuzzleSocketName))))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
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

	const FVector AimDirection = (Target->GetActorLocation() - TraceStart).GetSafeNormal();
	if (AimDirection.IsNearlyZero())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive())
	{
		return;
	}

	FHitResult Hit;
	if (IsValid(Monster) && IsValid(Target)
		&& Monster->LineTraceByChannel(Hit, TraceStart, TraceStart + AimDirection * TraceRange, HitscanTraceChannel)
		&& Hit.GetActor() == Target)
	{
		ITBHitInterface::Execute_ReceiveAttackHit(Target, ShotDamage, Monster, Hit);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
