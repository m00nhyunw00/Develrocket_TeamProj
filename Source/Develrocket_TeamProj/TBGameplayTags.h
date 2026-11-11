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
	// 도약 쿨다운 Gameplay Effect가 부여하는 재사용 제한 태그
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_MonsterLeap);

	// 전투 Ability ----------------------

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Combat);
	// 도약 어빌리티 식별 및 BT 타깃 전달 이벤트 태그
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Combat_MonsterLeap);
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
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Heal);
}
