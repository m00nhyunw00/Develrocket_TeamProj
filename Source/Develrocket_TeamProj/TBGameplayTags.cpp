#include "TBGameplayTags.h"

namespace TBGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG(State_Dead, "State.Dead");
	UE_DEFINE_GAMEPLAY_TAG(State_Stunned, "State.Stunned");
	UE_DEFINE_GAMEPLAY_TAG(State_MovementBlocked, "State.MovementBlocked");
	UE_DEFINE_GAMEPLAY_TAG(State_Invulnerable, "State.Invulnerable");
	UE_DEFINE_GAMEPLAY_TAG(State_Bleeding, "State.Bleeding");
	UE_DEFINE_GAMEPLAY_TAG(State_Combat, "State.Combat");
	UE_DEFINE_GAMEPLAY_TAG(State_Draining, "State.Draining");
	UE_DEFINE_GAMEPLAY_TAG(State_Aiming, "State.Aiming");
	UE_DEFINE_GAMEPLAY_TAG(State_Sprinting, "State.Sprinting");
	UE_DEFINE_GAMEPLAY_TAG(State_Empowered, "State.Empowered");
	
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_PrimaryAttack, "Cooldown.PrimaryAttack");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Skill1, "Cooldown.Skill1");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Skill2, "Cooldown.Skill2");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_MonsterSkill, "Cooldown.MonsterSkill");

	UE_DEFINE_GAMEPLAY_TAG(Ability_Combat, "Ability.Combat");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Combat_MonsterLeap, "Ability.Combat.MonsterLeap");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Combat_MonsterBoom, "Ability.Combat.MonsterBoom");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Combat_PrimaryAttack, "Ability.Combat.PrimaryAttack");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Combat_SecondaryAction, "Ability.Combat.SecondaryAction");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Combat_Skill1, "Ability.Combat.Skill1");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Combat_Skill2, "Ability.Combat.Skill2");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Combat_Melee, "Ability.Combat.Melee");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Combat_Reload, "Ability.Combat.Reload");

	UE_DEFINE_GAMEPLAY_TAG(Ability_Combat_Mobility, "Ability.Combat.Mobility");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Death, "Ability.Death");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Interaction, "Ability.Interaction");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Interaction_Drain, "Ability.Interaction.Drain");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Interaction_Instant, "Ability.Interaction.Instant");
	
	UE_DEFINE_GAMEPLAY_TAG(Data_Reload_BloodCost, "Data.Reload.BloodCost");
	UE_DEFINE_GAMEPLAY_TAG(Data_Reload_LoadedGain, "Data.Reload.LoadedGain");
	UE_DEFINE_GAMEPLAY_TAG(Data_Damage, "Data.Damage");
	UE_DEFINE_GAMEPLAY_TAG(Data_Heal, "Data.Heal");
	UE_DEFINE_GAMEPLAY_TAG(Data_MoveSpeedBonus, "Data.MoveSpeedBonus");
	
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Attack, "GameplayCue.Attack");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Attack_Weapon_BloodTrail, "GameplayCue.Attack.Weapon.BloodTrail");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Impact_Blood_World, "GameplayCue.Impact.Blood.World");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Impact_Blood_Flesh_Red, "GameplayCue.Impact.Blood.Flesh.Red");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Impact_Blood_Flesh_Green, "GameplayCue.Impact.Blood.Flesh.Green");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Blackwood_Attack, "GameplayCue.Blackwood.Attack");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Blackwood_Attack_Fire, "GameplayCue.Blackwood.Attack.Fire");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Blackwood_Attack_Melee, "GameplayCue.Blackwood.Attack.Melee");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Blackwood_Empowered, "GameplayCue.Blackwood.Empowered");
}
