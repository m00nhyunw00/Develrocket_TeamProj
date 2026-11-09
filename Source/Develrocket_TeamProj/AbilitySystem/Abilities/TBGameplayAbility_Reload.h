#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/TBGameplayAbility_Action.h"
#include "TBGameplayAbility_Reload.generated.h"

UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_Reload : public UTBGameplayAbility_Action
{
	GENERATED_BODY()

public:
	UTBGameplayAbility_Reload();

	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

protected:
	virtual void HandleActionMontageCompleted() override;
};