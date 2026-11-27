#pragma once

#include "CoreMinimal.h"
#include "Character/Player/TBPlayer.h"
#include "TBCrow.generated.h"

// 공통 플레이어 기능을 사용하는 크로우 캐릭터
UCLASS()
class DEVELROCKET_TEAMPROJ_API ATBCrow : public ATBPlayer
{
	GENERATED_BODY()

public:
	// 부모 클래스의 플레이어 구성 초기화
	ATBCrow(const FObjectInitializer& ObjectInitializer);
};
