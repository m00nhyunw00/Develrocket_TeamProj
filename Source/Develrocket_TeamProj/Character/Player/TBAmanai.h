#pragma once

#include "CoreMinimal.h"
#include "Character/Player/TBPlayer.h"
#include "TBAmanai.generated.h"

UCLASS()
class DEVELROCKET_TEAMPROJ_API ATBAmanai : public ATBPlayer
{
	GENERATED_BODY()

public:
	ATBAmanai(const FObjectInitializer& ObjectInitializer);
};