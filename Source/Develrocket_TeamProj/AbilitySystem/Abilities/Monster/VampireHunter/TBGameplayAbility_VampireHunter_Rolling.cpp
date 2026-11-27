#include "AbilitySystem/Abilities/Monster/VampireHunter/TBGameplayAbility_VampireHunter_Rolling.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_RollingCooldown, "Cooldown.Hunter.Rolling");

namespace
{
	const FName RollingLoopSection(TEXT("Loop"));
	constexpr int32 RollingLoopPlays = 5;
}

bool UTBGameplayAbility_VampireHunter_Rolling::CanUseAgainst(const ATBMonster* Monster, const AActor* Target) const
{
	return Super::CanUseAgainst(Monster, Target) && ThirdPersonMontage->bEnableAutoBlendOut
		&& ThirdPersonMontage->IsValidSectionName(RollingLoopSection)
		&& ThirdPersonMontage->GetSectionLength(ThirdPersonMontage->GetSectionIndex(RollingLoopSection)) > UE_KINDA_SMALL_NUMBER;
}

void UTBGameplayAbility_VampireHunter_Rolling::ActivateAbility(FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive())
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();
	if (!AnimInstance || !ASC || ASC->GetAnimatingAbility() != this || ASC->GetCurrentMontage() != ThirdPersonMontage)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	RollingAnimInstance = AnimInstance;
	FOnMontageSectionChanged SectionChanged;
	SectionChanged.BindUObject(this, &ThisClass::OnRollingSectionChanged);
	AnimInstance->Montage_SetSectionChangedDelegate(SectionChanged, ThirdPersonMontage);

	const FName StartSection = AnimInstance->Montage_GetCurrentSection(ThirdPersonMontage);
	ASC->CurrentMontageSetNextSectionName(RollingLoopSection, StartSection);
	if (StartSection == RollingLoopSection)
	{
		LoopPlayCount = 1;
	}
	else
	{
		ASC->CurrentMontageSetNextSectionName(StartSection, RollingLoopSection);
	}
}

void UTBGameplayAbility_VampireHunter_Rolling::OnRollingSectionChanged(UAnimMontage* Montage, FName SectionName, bool bLooped)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!IsActive() || Montage != ThirdPersonMontage || SectionName != RollingLoopSection
		|| !ASC || ASC->GetAnimatingAbility() != this || ASC->GetCurrentMontage() != Montage)
	{
		return;
	}

	if (++LoopPlayCount == RollingLoopPlays)
	{
		ASC->CurrentMontageSetNextSectionName(RollingLoopSection, NAME_None);
	}
}

void UTBGameplayAbility_VampireHunter_Rolling::EndAbility(FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UAnimInstance* AnimInstance = RollingAnimInstance.Get())
	{
		if (FOnMontageSectionChanged* SectionChanged = AnimInstance->Montage_GetSectionChangedDelegate(ThirdPersonMontage);
			SectionChanged && SectionChanged->IsBoundToObject(this))
		{
			SectionChanged->Unbind();
		}
	}
	RollingAnimInstance.Reset();
	LoopPlayCount = 0;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

const FGameplayTagContainer& UTBGameplayAbility_VampireHunter_Rolling::GetConfiguredSkillCooldownTags() const
{
	static const FGameplayTagContainer Tags(TAG_RollingCooldown);
	return Tags;
}
