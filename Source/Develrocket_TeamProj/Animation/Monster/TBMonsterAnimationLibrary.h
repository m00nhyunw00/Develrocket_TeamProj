#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "TBMonsterAnimationLibrary.generated.h"

UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBMonsterAnimationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Monster|Animation", meta = (BlueprintThreadSafe))
	static float CalculateLocomotionPlayRate(float GroundSpeed, float MinMovingSampleSpeed, float MaxMovingSampleSpeed);
};
