#include "AbilitySystem/Abilities/Monster/VampireHunter/TBGameplayAbility_VampireHunter_Dodge.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToForce.h"
#include "AIController.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Character/Monster/TBHunterAnimationData.h"
#include "Character/Monster/TBVampireHunter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TBHunterCombatComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "NavigationSystem.h"
#include "TBGameplayTags.h"

namespace
{
	const FName DodgeMotionName(TEXT("HunterDodge"));
	constexpr float DodgeMoveDuration = 0.20f;
	constexpr float DodgeTrailDuration = 0.32f;
}

UTBGameplayAbility_VampireHunter_Dodge::UTBGameplayAbility_VampireHunter_Dodge()
{
	bRetriggerInstancedAbility = true;
	SetAssetTags(FGameplayTagContainer(TBGameplayTags::Ability_Combat_Mobility));
	ActivationOwnedTags.AddTag(TBGameplayTags::State_Combat);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_MovementBlocked);
}

bool UTBGameplayAbility_VampireHunter_Dodge::CanActivateAbility(FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	const ATBVampireHunter* Hunter = ActorInfo ? Cast<ATBVampireHunter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!IsValid(Hunter) || !Hunter->HasAuthority() || !IsValid(ASC) || Hunter->IsActionBlocked()
		|| Hunter->GetHunterCombatComponent()->GetPhase() != ETBHunterPhase::Sword || !ActorInfo->GetAnimInstance())
	{
		return false;
	}

	const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle);
	const UGameplayAbility* AnimatingAbility = ASC->GetAnimatingAbility();
	const bool bAlreadyDodging = Spec && Spec->IsActive();
	const int32 CombatTagCount = ASC->GetTagCount(TBGameplayTags::State_Combat);
	const UTBHunterAnimationData* Data = Hunter->GetPhaseAnimationData(true);
	return Data && (!Data->SwordDodgeAnimations.IsEmpty() || !Data->SwordDodgeSequences.IsEmpty())
		&& !Data->SwordFullBodySlot.IsNone()
		&& CombatTagCount <= (bAlreadyDodging ? 1 : 0)
		&& (!AnimatingAbility || (bAlreadyDodging && AnimatingAbility == Spec->GetPrimaryInstance()))
		&& Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UTBGameplayAbility_VampireHunter_Dodge::ActivateAbility(FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	ATBVampireHunter* Hunter = ActorInfo ? Cast<ATBVampireHunter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UTBHunterAnimationData* Data = Hunter ? Hunter->GetPhaseAnimationData(true) : nullptr;
	TArray<FTBHunterDodgeAnimation> Candidates;
	const auto AddCandidate = [&Candidates](UAnimSequence* Sequence, float RightOffset)
	{
		if (IsValid(Sequence) && Sequence->GetSkeleton() && Sequence->GetPlayLength() > UE_KINDA_SMALL_NUMBER
			&& FMath::IsFinite(Sequence->RateScale) && Sequence->RateScale > 0.f && !Sequence->IsValidAdditive()
			&& FMath::IsFinite(RightOffset) && !Candidates.ContainsByPredicate([Sequence](const FTBHunterDodgeAnimation& Entry)
			{
				return Entry.Sequence == Sequence;
			}))
		{
			Candidates.Add({Sequence, RightOffset});
		}
	};
	if (Data)
	{
		if (Data->SwordDodgeAnimations.IsEmpty())
		{
			for (UAnimSequence* Sequence : Data->SwordDodgeSequences)
			{
				AddCandidate(Sequence, 0.f);
			}
		}
		else
		{
			for (const FTBHunterDodgeAnimation& Entry : Data->SwordDodgeAnimations)
			{
				AddCandidate(Entry.Sequence, Entry.RightOffset);
			}
		}
	}
	if (Candidates.Num() > 1)
	{
		Candidates.RemoveAllSwap([this](const FTBHunterDodgeAnimation& Entry)
		{
			return Entry.Sequence == LastDodgeSequence.Get();
		});
	}
	if (Candidates.IsEmpty())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FTBHunterDodgeAnimation& Selected = Candidates[FMath::RandHelper(Candidates.Num())];
	UAnimSequence* Sequence = Selected.Sequence;
	ThirdPersonMontage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(Sequence, Data->SwordFullBodySlot, 0.08f, 0.12f);
	if (!ThirdPersonMontage)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (IsActive())
	{
		LastDodgeSequence = Sequence;
		const float AnimationDuration = Sequence->GetPlayLength() / Sequence->RateScale;
		const FVector Offset = Hunter->GetActorRightVector().GetSafeNormal2D() * Selected.RightOffset;
		AAIController* Controller = Cast<AAIController>(Hunter->GetController());
		if (Controller && !Offset.IsNearlyZero() && Hunter->GetCharacterMovement()->IsMovingOnGround())
		{
			const FVector NavOrigin = Controller->GetNavAgentLocation();
			FVector NavHit;
			if (!UNavigationSystemV1::NavigationRaycast(Hunter, NavOrigin, NavOrigin + Offset, NavHit, nullptr, Controller))
			{
				auto* MoveTask = UAbilityTask_ApplyRootMotionMoveToForce::ApplyRootMotionMoveToForce(this, DodgeMotionName,
					Hunter->GetActorLocation() + Offset, FMath::Min(DodgeMoveDuration, AnimationDuration), false, MOVE_Walking, true,
					nullptr, ERootMotionFinishVelocityMode::SetVelocity, FVector::ZeroVector, 0.f);
				MoveTask->ReadyForActivation();
			}
		}
		bStartedMotionTrail = true;
		Hunter->GetHunterCombatComponent()->MulticastStartMotionTrail(FMath::Min(DodgeTrailDuration, AnimationDuration));
	}
}

void UTBGameplayAbility_VampireHunter_Dodge::OnActionCommitted()
{
	ATBVampireHunter* Hunter = CastChecked<ATBVampireHunter>(GetAvatarActorFromActorInfo());
	if (AAIController* Controller = Cast<AAIController>(Hunter->GetController()))
	{
		Controller->StopMovement();
	}
	Hunter->GetCharacterMovement()->StopMovementImmediately();
	Hunter->GetHunterCombatComponent()->SetSwordAnimation(ETBHunterSwordAnimation::Idle);
	if (USkeletalMeshComponent* Mesh = Hunter->GetMesh())
	{
		DodgeMesh = Mesh;
		PreviousVisibilityTickOption = static_cast<uint8>(Mesh->VisibilityBasedAnimTickOption);
		Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}
}

void UTBGameplayAbility_VampireHunter_Dodge::EndAbility(FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bStartedMotionTrail)
	{
		if (ATBVampireHunter* Hunter = Cast<ATBVampireHunter>(GetAvatarActorFromActorInfo()))
		{
			Hunter->GetHunterCombatComponent()->MulticastStopMotionTrail();
		}
		bStartedMotionTrail = false;
	}
	if (USkeletalMeshComponent* Mesh = DodgeMesh.Get())
	{
		Mesh->VisibilityBasedAnimTickOption = static_cast<EVisibilityBasedAnimTickOption>(PreviousVisibilityTickOption);
	}
	DodgeMesh.Reset();
	ThirdPersonMontage = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
