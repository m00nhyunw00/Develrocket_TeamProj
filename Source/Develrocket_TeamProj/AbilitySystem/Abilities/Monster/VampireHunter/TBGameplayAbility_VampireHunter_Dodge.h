#pragma once

#include "AbilitySystem/Abilities/TBGameplayAbility_Action.h"
#include "TBGameplayAbility_VampireHunter_Dodge.generated.h"

class UAnimSequence;
class USkeletalMeshComponent;

UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_VampireHunter_Dodge : public UTBGameplayAbility_Action
{
	GENERATED_BODY()

public:
	UTBGameplayAbility_VampireHunter_Dodge();
	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags,
		FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	virtual void OnActionCommitted() override;
	virtual bool ShouldAllowMontageInterruptAfterBlendOut() const override { return true; }

private:
	TWeakObjectPtr<UAnimSequence> LastDodgeSequence;
	TWeakObjectPtr<USkeletalMeshComponent> DodgeMesh;
	uint8 PreviousVisibilityTickOption = 0;
	bool bStartedMotionTrail = false;
};
