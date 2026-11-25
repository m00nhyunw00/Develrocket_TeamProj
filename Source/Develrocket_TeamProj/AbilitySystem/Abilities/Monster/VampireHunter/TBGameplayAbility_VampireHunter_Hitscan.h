#pragma once

#include "AbilitySystem/Abilities/Monster/TBGameplayAbility_MonsterSkill.h"
#include "TimerManager.h"
#include "TBGameplayAbility_VampireHunter_Hitscan.generated.h"

UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_VampireHunter_Hitscan : public UTBGameplayAbility_MonsterSkill
{
	GENERATED_BODY()

public:
	UTBGameplayAbility_VampireHunter_Hitscan();

	virtual bool CanUseAgainst(const ATBMonster* Monster, const AActor* Target) const override;
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	virtual bool ShouldWaitForMontageEnd() const override { return false; }

	UPROPERTY(EditDefaultsOnly, Category = "Hunter|Hitscan", meta = (ClampMin = "1.0", Units = "cm"))
	float TraceRange = 1200.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Hunter|Hitscan", meta = (ClampMin = "0.0"))
	float ShotDamage = 20.0f;

	// Leave empty to start at the Hunter's eyes. Otherwise use a socket on the character mesh.
	UPROPERTY(EditDefaultsOnly, Category = "Hunter|Hitscan")
	FName MuzzleSocketName;

private:
	void LockAim();
	void FireShot();

	TWeakObjectPtr<AActor> ShotTarget;
	TOptional<FVector> LockedAimLocation;
	FTimerHandle AimLockTimer;
	FTimerHandle FireTimer;
};
