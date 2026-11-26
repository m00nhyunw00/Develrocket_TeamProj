#include "AbilitySystem/Abilities/Monster/TBGameplayAbility_MonsterSkill.h"

#include "GameplayEffect.h"
#include "TBGameplayTags.h"

UGameplayEffect* UTBGameplayAbility_MonsterSkill::GetCooldownGameplayEffect() const
{
	return SkillCooldownEffectClass ? SkillCooldownEffectClass.GetDefaultObject() : Super::GetCooldownGameplayEffect();
}

bool UTBGameplayAbility_MonsterSkill::HasConfiguredSkillCooldown() const
{
	const UGameplayEffect* Effect = SkillCooldownEffectClass.GetDefaultObject();
	return Effect && Effect->DurationPolicy == EGameplayEffectDurationType::HasDuration
		&& FMath::IsFinite(SkillCooldownDuration) && SkillCooldownDuration > 0.f;
}

const FGameplayTagContainer* UTBGameplayAbility_MonsterSkill::GetCooldownTags() const
{
	if (!HasConfiguredSkillCooldown())
	{
		return Super::GetCooldownTags();
	}

	return &GetConfiguredSkillCooldownTags();
}

const FGameplayTagContainer& UTBGameplayAbility_MonsterSkill::GetConfiguredSkillCooldownTags() const
{
	static const FGameplayTagContainer CooldownTags(TBGameplayTags::Cooldown_MonsterSkill);
	return CooldownTags;
}

void UTBGameplayAbility_MonsterSkill::ApplyCooldown(FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo) const
{
	if (!HasConfiguredSkillCooldown())
	{
		if (!SkillCooldownEffectClass)
		{
			Super::ApplyCooldown(Handle, ActorInfo, ActivationInfo);
		}
		return;
	}

	const UGameplayEffect* Effect = GetCooldownGameplayEffect();
	const FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(
		Handle, ActorInfo, ActivationInfo, Effect->GetClass(), GetAbilityLevel(Handle, ActorInfo));
	if (Spec.IsValid())
	{
		Spec.Data->SetDuration(SkillCooldownDuration, true);
		Spec.Data->DynamicGrantedTags.AppendTags(GetConfiguredSkillCooldownTags());
		ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
	}
}
