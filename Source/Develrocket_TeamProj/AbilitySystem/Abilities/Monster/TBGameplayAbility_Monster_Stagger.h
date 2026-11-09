#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/TBGameplayAbility_Action.h"
#include "TBGameplayAbility_Monster_Stagger.generated.h"

UCLASS(Blueprintable)
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_Monster_Stagger : public UTBGameplayAbility_Action
{
	GENERATED_BODY()

public:
	UTBGameplayAbility_Monster_Stagger();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
