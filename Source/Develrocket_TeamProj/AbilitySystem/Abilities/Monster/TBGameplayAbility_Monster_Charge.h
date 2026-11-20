#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/Monster/TBGameplayAbility_MonsterSkill.h"
#include "Components/SkinnedMeshComponent.h"
#include "GameplayEffect.h"
#include "TBGameplayAbility_Monster_Charge.generated.h"

class ATBMonster;
class AActor;

UCLASS(NotBlueprintable)
class UTBGameplayEffect_ChargeWindup : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UTBGameplayEffect_ChargeWindup();
};

// Rage and Charge sections belong to the same montage. Charge must loop.
UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_Monster_Charge : public UTBGameplayAbility_MonsterSkill
{
	GENERATED_BODY()
public:
	UTBGameplayAbility_Monster_Charge();
	virtual bool CanUseAgainst(const ATBMonster* Monster, const AActor* Target) const override;
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category="Charge|Animation")
	FName ChargeSection = TEXT("Charge");
	UPROPERTY(EditDefaultsOnly, Category="Charge", meta=(ClampMin="1.0", Units="cm/s"))
	float ChargeSpeed = 1000.f;
	UPROPERTY(EditDefaultsOnly, Category="Charge", meta=(ClampMin="0.1", Units="s"))
	float MaxDuration = 2.f;
	UPROPERTY(EditDefaultsOnly, Category="Charge", meta=(ClampMin="1.0", Units="cm"))
	float MaxDistance = 1500.f;
	UPROPERTY(EditDefaultsOnly, Category="Charge", meta=(ClampMin="0.0"))
	float AttackDamage = 20.f;

private:
	void UpdateCharge();
	void FinishCharge(bool bCancelled);
	void RemoveWindupEffect();
	FActiveGameplayEffectHandle WindupEffectHandle;
	TWeakObjectPtr<ATBMonster> ChargingMonster;
	TWeakObjectPtr<AActor> ChargeTarget;
	FTimerHandle ChargeTimer;
	FVector StartLocation = FVector::ZeroVector;
	double ChargeStartTime = 0.0;
	float OriginalMaxWalkSpeed = 0.f;
	EVisibilityBasedAnimTickOption OriginalAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	bool bSavedAnimTickOption = false;
	bool bCharging = false;
};
