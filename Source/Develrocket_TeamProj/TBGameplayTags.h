#pragma once

#include "NativeGameplayTags.h"

namespace TBGameplayTags
{
	// 캐릭터 상태 ----------------------

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Stunned);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_MovementBlocked);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Combat);
	
	// 쿨다운 ----------------------
	
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_PrimaryAttack);

	// 전투 Ability ----------------------

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Combat);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Combat_PrimaryAttack);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Combat_Reload);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Death);
	
	// 상호작용 Ability ----------------------
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Interaction);
	
	// 데미지 타입 ----------------------
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Type_Bullet);
	
	// 데이터 타입 (SetByCaller용) ----------------------
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Reload_BloodCost);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Reload_LoadedGain);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage);
}