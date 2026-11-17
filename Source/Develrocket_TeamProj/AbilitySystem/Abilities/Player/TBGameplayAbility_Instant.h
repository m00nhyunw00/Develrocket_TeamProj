#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/Player/TBGameplayAbility_Interaction.h"
#include "TBGameplayAbility_Instant.generated.h"

UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_Instant : public UTBGameplayAbility_Interaction
{
	GENERATED_BODY()

public:
	UTBGameplayAbility_Instant();

protected:
	// 시작하자마자 결과 처리
	virtual bool ShouldResolveOnStart() const override;
};