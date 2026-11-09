#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/TBGameplayAbility_Action.h"
#include "TBGameplayAbility_PrimaryAttack.generated.h"

UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_PrimaryAttack : public UTBGameplayAbility_Action
{
	GENERATED_BODY()

public:
	UTBGameplayAbility_PrimaryAttack();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};