#pragma once

#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"
#include "TBGameplayAbility_BloodPulse.generated.h"

UCLASS()
class UTBGameplayEffect_BloodPulseCost : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UTBGameplayEffect_BloodPulseCost();
};

UCLASS()
class UTBGameplayEffect_BloodPulseCooldown : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UTBGameplayEffect_BloodPulseCooldown();
};

UCLASS(Abstract, Blueprintable)
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_BloodPulse : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UTBGameplayAbility_BloodPulse();
	virtual bool CheckCost(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual void ApplyCooldown(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Blood Pulse", meta = (ClampMin = "0.0"))
	float BloodCost = 15.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Blood Pulse", meta = (ClampMin = "0.1", Units = "s"))
	float CooldownSeconds = 8.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Blood Pulse", meta = (ClampMin = "1.0", Units = "cm"))
	float MaxRadius = 1800.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Blood Pulse", meta = (ClampMin = "0.05", Units = "s"))
	float ExpansionSeconds = 0.75f;

	UPROPERTY(EditDefaultsOnly, Category = "Blood Pulse", meta = (ClampMin = "0.05", Units = "s"))
	float PlayerRevealSeconds = 0.75f;

	UPROPERTY(EditDefaultsOnly, Category = "Blood Pulse", meta = (ClampMin = "0.05", Units = "s"))
	float OtherRevealSeconds = 2.0f;

private:
	void AdvanceWave();

	FTimerHandle WaveTimer;
	FVector Origin = FVector::ZeroVector;
	double StartTime = 0.0;
	TSet<TWeakObjectPtr<AActor>> RevealedActors;
};
