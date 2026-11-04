#include "Character/Monster/GAS/TBMonsterGameplayEffects.h"

#include "Character/Monster/GAS/TBMonsterAttributeSet.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "TBGameplayTags.h"
#include "Character/Monster/GAS/TBMonsterGameplayTags.h"

UTBGameplayEffect_MonsterInitialize::UTBGameplayEffect_MonsterInitialize()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo MaxHealthModifier;
	MaxHealthModifier.Attribute = UTBMonsterAttributeSet::GetMaxHealthAttribute();
	MaxHealthModifier.ModifierOp = EGameplayModOp::Override;
	MaxHealthModifier.ModifierMagnitude = FScalableFloat(100.0f);
	Modifiers.Add(MaxHealthModifier);

	FGameplayModifierInfo HealthModifier;
	HealthModifier.Attribute = UTBMonsterAttributeSet::GetHealthAttribute();
	HealthModifier.ModifierOp = EGameplayModOp::Override;
	HealthModifier.ModifierMagnitude = FScalableFloat(100.0f);
	Modifiers.Add(HealthModifier);
}

UTBGameplayEffect_MonsterDamage::UTBGameplayEffect_MonsterDamage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat DamageMagnitude;
	DamageMagnitude.DataTag = TBMonsterGameplayTags::Data_Damage;

	FGameplayModifierInfo DamageModifier;
	DamageModifier.Attribute = UTBMonsterAttributeSet::GetIncomingDamageAttribute();
	DamageModifier.ModifierOp = EGameplayModOp::Additive;
	DamageModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(DamageMagnitude);
	Modifiers.Add(DamageModifier);
}

UTBGameplayEffect_MonsterDeath::UTBGameplayEffect_MonsterDeath()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// Minimal ASC에서도 사망 상태 태그 복제
	FInheritedTagContainer DeathTags;
	DeathTags.Added.AddTag(TBGameplayTags::State_Dead);
	UTargetTagsGameplayEffectComponent* TargetTags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("DeathStateTags"));
	GEComponents.Add(TargetTags);
	TargetTags->SetAndApplyTargetTagChanges(DeathTags);
}
