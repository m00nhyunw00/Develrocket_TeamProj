#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectUIData.h"
#include "Abilities/GameplayAbility.h"
#include "TBGameplayAbility_Interaction.generated.h"

UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_Interaction : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UTBGameplayAbility_Interaction();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};