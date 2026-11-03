#pragma once

#include "NativeGameplayTags.h"

namespace TBGameplayTags
{
	// 캐릭터 상태 ----------------------

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Stunned);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_MovementBlocked);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Combat);

	// 전투 Ability ----------------------

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Combat);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Combat_PrimaryAttack);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Combat_Reload);
	
	// 데이터 타입 ----------------------
	
	// 재장전 GE에 전달할 보유 혈액 차감량과 탄창 충전량
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Reload_BloodCost);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Reload_LoadedGain);
}