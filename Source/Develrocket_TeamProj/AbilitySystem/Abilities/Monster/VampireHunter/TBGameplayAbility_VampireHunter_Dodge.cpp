#include "AbilitySystem/Abilities/Monster/VampireHunter/TBGameplayAbility_VampireHunter_Dodge.h"

#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Character/Monster/TBHunterAnimationData.h"
#include "Character/Monster/TBVampireHunter.h"
#include "Components/TBHunterCombatComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TBGameplayTags.h"

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
	return Data && !Data->SwordDodgeSequences.IsEmpty() && !Data->SwordFullBodySlot.IsNone()
		&& CombatTagCount <= (bAlreadyDodging ? 1 : 0)
		&& (!AnimatingAbility || (bAlreadyDodging && AnimatingAbility == Spec->GetPrimaryInstance()))
		&& Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UTBGameplayAbility_VampireHunter_Dodge::ActivateAbility(FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	const ATBVampireHunter* Hunter = ActorInfo ? Cast<ATBVampireHunter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UTBHunterAnimationData* Data = Hunter ? Hunter->GetPhaseAnimationData(true) : nullptr;
	TArray<UAnimSequence*> Candidates;
	if (Data)
	{
		for (UAnimSequence* Sequence : Data->SwordDodgeSequences)
		{
			if (IsValid(Sequence) && Sequence->GetSkeleton() && Sequence->GetPlayLength() > UE_KINDA_SMALL_NUMBER
				&& FMath::IsFinite(Sequence->RateScale) && Sequence->RateScale > 0.f && !Sequence->IsValidAdditive())
			{
				Candidates.AddUnique(Sequence);
			}
		}
	}
	if (Candidates.Num() > 1)
	{
		Candidates.RemoveSingleSwap(LastDodgeSequence.Get());
	}
	if (Candidates.IsEmpty())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAnimSequence* Sequence = Candidates[FMath::RandHelper(Candidates.Num())];
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
}

void UTBGameplayAbility_VampireHunter_Dodge::EndAbility(FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	ThirdPersonMontage = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
