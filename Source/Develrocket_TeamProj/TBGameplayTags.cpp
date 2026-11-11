#include "TBGameplayTags.h"

namespace TBGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG(Character_Type_Player, "Character.Type.Player");
	UE_DEFINE_GAMEPLAY_TAG(Character_Type_Monster, "Character.Type.Monster");
	UE_DEFINE_GAMEPLAY_TAG(Character_Type_NPC, "Character.Type.NPC");
	
	
	UE_DEFINE_GAMEPLAY_TAG(State_Dead, "State.Dead");
	UE_DEFINE_GAMEPLAY_TAG(State_Stunned, "State.Stunned");
	UE_DEFINE_GAMEPLAY_TAG(State_MovementBlocked, "State.MovementBlocked");
	UE_DEFINE_GAMEPLAY_TAG(State_Combat, "State.Combat");
	
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_PrimaryAttack, "Cooldown.PrimaryAttack");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_MonsterLeap, "Cooldown.MonsterLeap");

	UE_DEFINE_GAMEPLAY_TAG(Ability_Combat, "Ability.Combat");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Combat_MonsterLeap, "Ability.Combat.MonsterLeap");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Combat_PrimaryAttack, "Ability.Combat.PrimaryAttack");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Combat_Reload, "Ability.Combat.Reload");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Death, "Ability.Death");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Interaction, "Ability.Interaction");
	
	UE_DEFINE_GAMEPLAY_TAG(Damage_Type_Bullet, "Ability.Type.Bullet");
	
	UE_DEFINE_GAMEPLAY_TAG(Data_Reload_BloodCost, "Data.Reload.BloodCost");
	UE_DEFINE_GAMEPLAY_TAG(Data_Reload_LoadedGain, "Data.Reload.LoadedGain");
	UE_DEFINE_GAMEPLAY_TAG(Data_Damage, "Data.Damage");
	UE_DEFINE_GAMEPLAY_TAG(Data_Heal, "Data.Heal");
	
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Weapon_BloodTrail, "GameplayCue.Weapon.BloodTrail");
}
