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
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	virtual void HandleActionMontageCompleted() override;

private:
	// [서버] Execute_Interact + Client_NotifyInteracted
	void ResolveInteraction();

	// 완료 시점까지 기억할 대상
	TWeakObjectPtr<AActor> CurrentTarget;
	
	// [서버] BeginInteract를 보냈는지, 결과 (Interact)까지 갔는지 -> 취소 알림이 필요한지 판단.
	bool bInteractionBegun = false;
	bool bInteractionResolved = false;
};