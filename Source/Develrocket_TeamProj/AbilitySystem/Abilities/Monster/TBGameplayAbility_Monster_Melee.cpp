#include "AbilitySystem/Abilities/Monster/TBGameplayAbility_Monster_Melee.h"

#include "TBGameplayTags.h"

UTBGameplayAbility_Monster_Melee::UTBGameplayAbility_Monster_Melee()
{
	SetAssetTags(FGameplayTagContainer(TBGameplayTags::Ability_Combat));
	ActivationOwnedTags.AddTag(TBGameplayTags::State_Combat);
	ActivationOwnedTags.AddTag(TBGameplayTags::State_MovementBlocked);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Stunned);
}
