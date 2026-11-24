#pragma once

#include "NativeGameplayTags.h"

namespace TBGameplayTags
{
	// 캐릭터 상태 ----------------------

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Stunned);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_MovementBlocked);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Invulnerable);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Bleeding);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Combat);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Draining);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Aiming);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Sprinting);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Empowered);
	
	// 쿨다운 ----------------------
	
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_PrimaryAttack);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill1);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill2);
	// 도약 쿨다운 Gameplay Effect가 부여하는 재사용 제한 태그
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_MonsterSkill);

	// 전투 Ability ----------------------

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Combat);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Combat_PrimaryAttack);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Combat_SecondaryAction);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Combat_Skill1);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Combat_Skill2);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Combat_Melee);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Combat_Reload);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Combat_Mobility);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Death);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Combat_MonsterLeap);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Combat_MonsterBoom);
	
	// 상호작용 Ability ----------------------
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Interaction);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Interaction_Drain);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Interaction_Instant);
	
	// 데이터 타입 (SetByCaller용) ----------------------
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Reload_BloodCost);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Reload_LoadedGain);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Heal);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_MoveSpeedBonus);

	
	// 총격 연출 ----------------------
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Weapon_BloodTrail);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Impact_Blood_World);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Impact_Blood_Flesh_Red);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Impact_Blood_Flesh_Green);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Blackwood_Fire);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Blackwood_Melee);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Blackwood_Empowered);
}
