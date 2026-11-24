#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/TBGameplayAbility_Action.h"
#include "TimerManager.h"
#include "TBGameplayAbility_HoldAction.generated.h"

class UGameplayEffect;

// 입력을 누르는 동안 효과를 유지하는 공통 어빌리티
UCLASS(Abstract, Blueprintable)
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_HoldAction
	: public UTBGameplayAbility_Action
{
	GENERATED_BODY()

public:
	UTBGameplayAbility_HoldAction();

	virtual void ActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

protected:
	// 입력 유지 중 적용할 Infinite 효과
	UPROPERTY(EditDefaultsOnly, Category = "Hold")
	TSubclassOf<UGameplayEffect> StateEffectClass;

	// 지정 시 서버에서 1초마다 적용할 혈액 1 소모 Instant 효과
	UPROPERTY(EditDefaultsOnly, Category = "Hold")
	TSubclassOf<UGameplayEffect> PeriodicCostEffectClass;

	virtual bool ShouldWaitForMontageEnd() const override
	{
		return false;
	}

private:
	FActiveGameplayEffectHandle StateEffectHandle;
	FTimerHandle CostTimer;

	void ApplyPeriodicCost();

	UFUNCTION()
	void OnInputReleased(float TimeHeld);

	UFUNCTION()
	void OnInterrupted();
};