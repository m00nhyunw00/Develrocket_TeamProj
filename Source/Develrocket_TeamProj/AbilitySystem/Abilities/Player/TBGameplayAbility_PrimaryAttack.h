#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/TBGameplayAbility_Action.h"
#include "TBGameplayAbility_PrimaryAttack.generated.h"

UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_PrimaryAttack : public UTBGameplayAbility_Action
{
	GENERATED_BODY()

public:
	UTBGameplayAbility_PrimaryAttack();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	// 일반 공격은 몽타주 완료를 기다리지 않음
	virtual bool ShouldWaitForMontageEnd() const override
	{
		return false;
	}

	// 일반 공격의 재활성화를 제한하는 태그
	virtual const FGameplayTagContainer* GetCooldownTags() const override;

	// PlayerState 설정으로 쿨다운 적용
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
};