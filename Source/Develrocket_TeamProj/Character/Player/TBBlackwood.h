#pragma once

#include "CoreMinimal.h"
#include "Character/Player/TBPlayer.h"
#include "TBBlackwood.generated.h"

UCLASS()
class DEVELROCKET_TEAMPROJ_API ATBBlackwood : public ATBPlayer
{
	GENERATED_BODY()

public:
	ATBBlackwood(const FObjectInitializer& ObjectInitializer);
};