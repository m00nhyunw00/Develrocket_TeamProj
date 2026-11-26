#pragma once

#include "AbilitySystem/Abilities/Monster/TBGameplayAbility_MonsterSkill.h"
#include "TBGameplayAbility_VampireHunter_SwordDash.generated.h"

class UAnimSequenceBase;
class USkeletalMeshComponent;

UCLASS(Blueprintable)
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_VampireHunter_SwordDash : public UTBGameplayAbility_MonsterSkill
{
	GENERATED_BODY()

public:
	UTBGameplayAbility_VampireHunter_SwordDash();
	virtual bool CanUseAgainst(const ATBMonster* Monster, const AActor* Target) const override;
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	void DashFromMontageNotify(const UAnimSequenceBase* Animation);

protected:
	virtual void OnActionCommitted() override;
	virtual void HandleActionMontageCompleted() override;
	virtual bool ShouldAllowMontageInterruptAfterBlendOut() const override { return true; }
	virtual const FGameplayTagContainer& GetConfiguredSkillCooldownTags() const override;

	UPROPERTY(EditDefaultsOnly, Category = "Hunter|Sword Dash", meta = (ClampMin = "1.0", Units = "cm/s"))
	float DashSpeed = 2000.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Hunter|Sword Dash", meta = (ClampMin = "1.0", Units = "cm"))
	float MaxDashDistance = 600.0f;

	// Additional clearance between the two collision capsules at the destination.
	UPROPERTY(EditDefaultsOnly, Category = "Hunter|Sword Dash", meta = (ClampMin = "0.0", Units = "cm"))
	float StopGap = 35.0f;

private:
	bool CanDashToward(const ATBMonster* Hunter, const AActor* Target) const;
	double GetStopDistance(const ATBMonster& Hunter, const AActor& Target) const;

	UFUNCTION()
	void OnDashFinished();

	UFUNCTION()
	void OnStunned();

	TWeakObjectPtr<const AActor> DashTarget;
	TWeakObjectPtr<USkeletalMeshComponent> NotifyMesh;
	uint8 PreviousVisibilityTickOption = 0;
	bool bDashTriggered = false;
	bool bDashInProgress = false;
	bool bMontageCompleted = false;
};
