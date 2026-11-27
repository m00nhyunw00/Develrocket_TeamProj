#include "AbilitySystem/Abilities/Monster/VampireHunter/TBGameplayAbility_VampireHunter_SwordDash.h"

#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToForce.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayTag.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Animation/AnimMontage.h"
#include "Animation/Monster/TBAnimNotify_HunterSwordDash.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/Monster/TBMonster.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TBHunterCombatComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "GameplayEffect.h"
#include "NativeGameplayTags.h"
#include "NavigationSystem.h"
#include "TBGameplayTags.h"
#include "UObject/ConstructorHelpers.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SwordDashCooldown, "Cooldown.Hunter.SwordDash");

namespace
{
const FName SwordDashMotionName(TEXT("HunterSwordDash"));
constexpr double MaxTargetHeightDifference = 125.0;
}

UTBGameplayAbility_VampireHunter_SwordDash::UTBGameplayAbility_VampireHunter_SwordDash()
{
	static const ConstructorHelpers::FClassFinder<UGameplayEffect> DefaultCooldownEffect(
		TEXT("/Game/_Game/Blueprints/AbilitySystem/Monster/GE_Monster_SkillCooldown"));
	SkillCooldownEffectClass = DefaultCooldownEffect.Class;
	SkillCooldownDuration = 6.0f;

	SetAssetTags(FGameplayTagContainer(TBGameplayTags::Ability_Combat_Mobility));
	ActivationOwnedTags.AddTag(TBGameplayTags::State_Combat);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Combat);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Stunned);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Draining);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_MovementBlocked);
}

const FGameplayTagContainer& UTBGameplayAbility_VampireHunter_SwordDash::GetConfiguredSkillCooldownTags() const
{
	static const FGameplayTagContainer Tags(TAG_SwordDashCooldown);
	return Tags;
}

bool UTBGameplayAbility_VampireHunter_SwordDash::CanUseAgainst(const ATBMonster* Monster, const AActor* Target) const
{
	if (!FMath::IsFinite(DashSpeed) || DashSpeed <= 0.f || !FMath::IsFinite(MaxDashDistance) || MaxDashDistance <= 0.f
		|| !FMath::IsFinite(StopGap) || StopGap < 0.f || !CanDashToward(Monster, Target))
	{
		return false;
	}

	const AAIController* Controller = Cast<AAIController>(Monster->GetController());
	return Controller && Controller->LineOfSightTo(Target) && Monster->GetMesh()->GetAnimInstance()
		&& ThirdPersonMontage && !ThirdPersonMontage->HasRootMotion() && FMath::IsFinite(ThirdPersonMontage->RateScale)
		&& ThirdPersonMontage->RateScale > 0.f && ThirdPersonMontage->GetPlayLength() > 0.f
		&& ThirdPersonMontage->Notifies.ContainsByPredicate([](const FAnimNotifyEvent& Event)
		{
			return Event.Notify && Event.Notify->IsA<UTBAnimNotify_HunterSwordDash>() && Event.bTriggerOnDedicatedServer;
		});
}

bool UTBGameplayAbility_VampireHunter_SwordDash::CanDashToward(const ATBMonster* Hunter, const AActor* Target) const
{
	const ATBCharacterBase* Character = Cast<ATBCharacterBase>(Target);
	if (!IsValid(Hunter) || !Hunter->HasAuthority() || Hunter->GetMonsterBehavior() != ETBMonsterBehavior::Hunter
		|| Hunter->IsActionBlocked() || Hunter->IsMovementBlocked() || !Hunter->GetCharacterMovement()->IsMovingOnGround()
		|| Hunter->GetHunterCombatComponent()->GetPhase() != ETBHunterPhase::Sword
		|| !IsValid(Character) || Character == Hunter || Character->GetWorld() != Hunter->GetWorld())
	{
		return false;
	}

	const UAbilitySystemComponent* TargetASC = Character->GetAbilitySystemComponent();
	const FVector Offset = Character->GetActorLocation() - Hunter->GetActorLocation();
	return IsValid(TargetASC) && TargetASC->GetAvatarActor() == Character
		&& !TargetASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead)
		&& FMath::Abs(Offset.Z) <= MaxTargetHeightDifference
		&& Offset.SizeSquared2D() > FMath::Square(GetStopDistance(*Hunter, *Character));
}

double UTBGameplayAbility_VampireHunter_SwordDash::GetStopDistance(const ATBMonster& Hunter, const AActor& Target) const
{
	return Hunter.GetSimpleCollisionRadius() + Target.GetSimpleCollisionRadius() + StopGap;
}

void UTBGameplayAbility_VampireHunter_SwordDash::ActivateAbility(FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	ATBMonster* Hunter = ActorInfo ? Cast<ATBMonster>(ActorInfo->AvatarActor.Get()) : nullptr;
	const AAIController* Controller = Hunter ? Cast<AAIController>(Hunter->GetController()) : nullptr;
	const UBlackboardComponent* Blackboard = Controller ? Controller->GetBlackboardComponent() : nullptr;
	const AActor* Target = TriggerEventData ? TriggerEventData->Target.Get() : nullptr;
	if (!Target && Blackboard)
	{
		static const FName SwordDashTargetKey(TEXT("TargetActor"));
		Target = Cast<AActor>(Blackboard->GetValueAsObject(SwordDashTargetKey));
	}

	if (!CanUseAgainst(Hunter, Target))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	DashTarget = Target;
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UTBGameplayAbility_VampireHunter_SwordDash::OnActionCommitted()
{
	ATBMonster* Hunter = CastChecked<ATBMonster>(GetAvatarActorFromActorInfo());
	Hunter->GetController()->StopMovement();
	Hunter->GetCharacterMovement()->StopMovementImmediately();
	Hunter->GetHunterCombatComponent()->SetSwordAnimation(ETBHunterSwordAnimation::Idle);

	if (const AActor* Target = DashTarget.Get())
	{
		const FVector Direction = (Target->GetActorLocation() - Hunter->GetActorLocation()).GetSafeNormal2D();
		if (!Direction.IsNearlyZero())
		{
			Hunter->SetActorRotation(Direction.Rotation());
		}
	}

	USkeletalMeshComponent* Mesh = Hunter->GetMesh();
	NotifyMesh = Mesh;
	PreviousVisibilityTickOption = static_cast<uint8>(Mesh->VisibilityBasedAnimTickOption);
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	auto* StunTask = UAbilityTask_WaitGameplayTagAdded::WaitGameplayTagAdd(this, TBGameplayTags::State_Stunned, nullptr, true);
	StunTask->Added.AddDynamic(this, &ThisClass::OnStunned);
	StunTask->ReadyForActivation();
}

void UTBGameplayAbility_VampireHunter_SwordDash::DashFromMontageNotify(const UAnimSequenceBase* Animation)
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!IsActive() || bDashTriggered || !Animation || Animation != ThirdPersonMontage.Get()
		|| !ASC || ASC->GetAnimatingAbility() != this || ASC->GetCurrentMontage() != ThirdPersonMontage.Get())
	{
		return;
	}

	bDashTriggered = true;
	ATBMonster* Hunter = Cast<ATBMonster>(GetAvatarActorFromActorInfo());
	const AActor* Target = DashTarget.Get();
	if (!CanDashToward(Hunter, Target))
	{
		return;
	}

	AAIController* Controller = Cast<AAIController>(Hunter->GetController());
	const FVector Offset = Target->GetActorLocation() - Hunter->GetActorLocation();
	const FVector Direction = Offset.GetSafeNormal2D();
	const double Distance = FMath::Min(Offset.Size2D() - GetStopDistance(*Hunter, *Target), static_cast<double>(MaxDashDistance));
	if (!Controller || Direction.IsNearlyZero() || Distance <= UE_KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector DashOffset = Direction * Distance;
	const FVector NavOrigin = Controller->GetNavAgentLocation();
	FVector NavHit;
	if (UNavigationSystemV1::NavigationRaycast(Hunter, NavOrigin, NavOrigin + DashOffset, NavHit, nullptr, Controller))
	{
		return;
	}

	Hunter->SetActorRotation(Direction.Rotation());
	bDashInProgress = true;
	auto* DashTask = UAbilityTask_ApplyRootMotionMoveToForce::ApplyRootMotionMoveToForce(this, SwordDashMotionName,
		Hunter->GetActorLocation() + DashOffset, static_cast<float>(Distance / DashSpeed), false, MOVE_Walking, true,
		nullptr, ERootMotionFinishVelocityMode::SetVelocity, FVector::ZeroVector, 0.f);
	DashTask->OnTimedOut.AddDynamic(this, &ThisClass::OnDashFinished);
	DashTask->OnTimedOutAndDestinationReached.AddDynamic(this, &ThisClass::OnDashFinished);
	DashTask->ReadyForActivation();
}

void UTBGameplayAbility_VampireHunter_SwordDash::HandleActionMontageCompleted()
{
	bMontageCompleted = true;
	if (!bDashInProgress)
	{
		Super::HandleActionMontageCompleted();
	}
}

void UTBGameplayAbility_VampireHunter_SwordDash::OnDashFinished()
{
	bDashInProgress = false;
	if (bMontageCompleted)
	{
		Super::HandleActionMontageCompleted();
	}
}

void UTBGameplayAbility_VampireHunter_SwordDash::OnStunned()
{
	CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
}

void UTBGameplayAbility_VampireHunter_SwordDash::EndAbility(FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (USkeletalMeshComponent* Mesh = NotifyMesh.Get())
	{
		Mesh->VisibilityBasedAnimTickOption = static_cast<EVisibilityBasedAnimTickOption>(PreviousVisibilityTickOption);
	}
	NotifyMesh.Reset();
	DashTarget.Reset();
	bDashTriggered = false;
	bDashInProgress = false;
	bMontageCompleted = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
