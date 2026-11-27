#pragma once

#include "AbilitySystem/Abilities/Monster/VampireHunter/TBGameplayAbility_VampireHunter_SwordDash.h"
#include "TBGameplayAbility_VampireHunter_Rolling.generated.h"

UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_VampireHunter_Rolling : public UTBGameplayAbility_VampireHunter_SwordDash
{
	GENERATED_BODY()

public:
	virtual bool CanUseAgainst(const ATBMonster* Monster, const AActor* Target) const override;
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	virtual const FGameplayTagContainer& GetConfiguredSkillCooldownTags() const override;

private:
	void OnRollingSectionChanged(UAnimMontage* Montage, FName SectionName, bool bLooped);

	TWeakObjectPtr<UAnimInstance> RollingAnimInstance;
	int32 LoopPlayCount = 0;
};
