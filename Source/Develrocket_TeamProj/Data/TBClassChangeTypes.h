#pragma once

#include "CoreMinimal.h"
#include "TBClassChangeTypes.generated.h"

// 직업 변경 결재 결과 (요청한 클라에 통보)
UENUM()
enum class ETBClassChangeResult : uint8
{
	Approved,
	NoChange,       // 이미 그 직업
	TravelStarted,  // 맵 이동 시작됨
	NotAllowed,     // 목록에 없는 직업
	Cooldown,       // 연타
	NotInLobby,     // 로비 목록에 없음 / 로비 맵 아님
};
