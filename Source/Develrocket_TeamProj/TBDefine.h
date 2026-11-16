#pragma once

#include "CoreMinimal.h"
#include "TBDefine.generated.h"

// 시민의 존재 상태
UENUM(BlueprintType)
enum class ETBCitizenState : uint8
{
	Idle UMETA(DisplayName = "Idle"),
	Drained UMETA(DisplayName = "Drained"),
	Dead UMETA(DisplayName = "Dead")
};
