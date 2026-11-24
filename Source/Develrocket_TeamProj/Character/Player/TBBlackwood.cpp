#include "Character/Player/TBBlackwood.h"

#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"
#include "TBGameplayTags.h"
#include "AbilitySystem/Abilities/Player/TBGameplayAbility_Empower.h"

ATBBlackwood::ATBBlackwood(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

UTBGameplayAbility_Empower* ATBBlackwood::GetActiveEmpower() const
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();

	if (!IsValid(ASC))
	{
		return nullptr;
	}

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		auto* Ability = Cast<UTBGameplayAbility_Empower>(Spec.GetPrimaryInstance());

		if (Ability && Ability->IsActive())
		{
			return Ability;
		}
	}

	return nullptr;
}

bool ATBBlackwood::ConsumeAttackEnhancement(const FGameplayTag& AttackId, float& InOutDamage)
{
	if (!HasAuthority() || (AttackId != TBGameplayTags::Ability_Combat_PrimaryAttack && AttackId != TBGameplayTags::Ability_Combat_Melee))
	{
		return false;
	}

	if (UTBGameplayAbility_Empower* Empower = GetActiveEmpower())
	{
		return Empower->ConsumeForAttack(InOutDamage);
	}

	return false;
}

float ATBBlackwood::ModifyIncomingAttackDamage(float Damage) const
{
	const float BaseDamage = Super::ModifyIncomingAttackDamage(Damage);

	if (const UTBGameplayAbility_Empower* Empower = GetActiveEmpower())
	{
		const float Reduction = FMath::Clamp(Empower->GetDamageReduction(), 0.0f, 1.0f);

		return BaseDamage * (1.0f - Reduction);
	}

	return BaseDamage;
}