#pragma once

#include "AbilitySystem/Abilities/Monster/VampireHunter/TBGameplayAbility_VampireHunter_Hitscan.h"
#include "TBGameplayAbility_VampireHunter_RifleSkills.generated.h"

class UAnimSequenceBase;
class USkeletalMeshComponent;

UCLASS(Abstract)
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_VampireHunter_StationaryShot : public UTBGameplayAbility_VampireHunter_Hitscan
{
	GENERATED_BODY()

public:
	UTBGameplayAbility_VampireHunter_StationaryShot();
	virtual bool CanUseAgainst(const ATBMonster* Monster, const AActor* Target) const override;
	virtual bool CanFireDuringCombatMovement() const override { return false; }

protected:
	virtual bool UsesPhaseAttackMontage() const override { return false; }
	virtual void OnActionCommitted() override;
	bool FireAtCurrentTarget() const;
};

UCLASS(Blueprintable)
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_VampireHunter_ChargedShot : public UTBGameplayAbility_VampireHunter_StationaryShot
{
	GENERATED_BODY()

public:
	UTBGameplayAbility_VampireHunter_ChargedShot();
	virtual bool CanUseAgainst(const ATBMonster* Monster, const AActor* Target) const override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	virtual void StartShotSequence() override;
	virtual const FGameplayTagContainer& GetConfiguredSkillCooldownTags() const override;

	UPROPERTY(EditDefaultsOnly, Category = "Hunter|Charge", meta = (ClampMin = "0.01", Units = "s"))
	float ChargeDuration = 3.0f;

private:
	void FinishCharge();
	FTimerHandle ChargeTimer;
};

UCLASS(Blueprintable)
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_VampireHunter_NotifyShot : public UTBGameplayAbility_VampireHunter_StationaryShot
{
	GENERATED_BODY()

public:
	UTBGameplayAbility_VampireHunter_NotifyShot();
	virtual bool CanUseAgainst(const ATBMonster* Monster, const AActor* Target) const override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	void FireFromMontageNotify(const UAnimSequenceBase* Animation) const;

protected:
	virtual bool ShouldWaitForMontageEnd() const override { return true; }
	virtual bool ShouldAllowMontageInterruptAfterBlendOut() const override { return true; }
	virtual void OnActionCommitted() override;
	virtual void StartShotSequence() override {}
	virtual const FGameplayTagContainer& GetConfiguredSkillCooldownTags() const override;

private:
	TWeakObjectPtr<USkeletalMeshComponent> NotifyMesh;
	uint8 PreviousVisibilityTickOption = 0;
};
