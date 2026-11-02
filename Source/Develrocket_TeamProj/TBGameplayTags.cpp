#include "TBGameplayTags.h"

namespace TBGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG(State_Dead, "State.Dead");
	UE_DEFINE_GAMEPLAY_TAG(State_Stunned, "State.Stunned");
	UE_DEFINE_GAMEPLAY_TAG(State_MovementBlocked, "State.MovementBlocked");
	UE_DEFINE_GAMEPLAY_TAG(State_Combat, "State.Combat");

	UE_DEFINE_GAMEPLAY_TAG(Ability_Combat, "Ability.Combat");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Combat_PrimaryAttack, "Ability.Combat.PrimaryAttack");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Combat_Reload, "Ability.Combat.Reload");
}