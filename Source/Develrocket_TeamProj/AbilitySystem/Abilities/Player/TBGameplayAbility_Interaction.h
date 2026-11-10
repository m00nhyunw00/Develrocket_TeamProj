#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/TBGameplayAbility_Action.h"
#include "TBGameplayAbility_Interaction.generated.h"

UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_Interaction : public UTBGameplayAbility_Action
{
	GENERATED_BODY()

public:
	UTBGameplayAbility_Interaction();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	virtual void HandleActionMontageCompleted() override;

private:
	// [서버] Execute_Interact + Client_NotifyInteracted
	void ResolveInteraction();

	// 완료 시점까지 기억할 대상
	TWeakObjectPtr<AActor> CurrentTarget;
};