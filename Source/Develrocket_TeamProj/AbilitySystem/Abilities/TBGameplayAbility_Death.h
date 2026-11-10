#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/TBGameplayAbility_Action.h"
#include "TBGameplayAbility_Death.generated.h"

UCLASS(Abstract)
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_Death
	: public UTBGameplayAbility_Action
{
	GENERATED_BODY()

public:
	UTBGameplayAbility_Death();
};