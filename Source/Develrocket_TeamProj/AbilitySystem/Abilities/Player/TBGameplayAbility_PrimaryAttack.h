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
	// 일반 공격에 사용할 총격 궤적 Cue
	UPROPERTY(EditDefaultsOnly, Category = "Effects", meta = (Categories = "GameplayCue"))
	FGameplayTag BulletTrailCueTag;
	
	// 벽·바닥 등 비생명체에 사용할 기본 피격 Cue
	UPROPERTY(EditDefaultsOnly, Category = "GAS|GameplayCue", meta = (Categories = "GameplayCue"))
	FGameplayTag DefaultImpactCueTag;
	
	// 한 발당 기본 반동 각도
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Recoil", meta = (ClampMin = "0.0"))
	float CameraRecoilPitch = 0.25f;

	// 최고점까지 상승하는 시간
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Recoil", meta = (ClampMin = "0.01"))
	float CameraRecoilRiseTime = 0.04f;

	// 최고점에서 복귀하는 시간
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Recoil", meta = (ClampMin = "0.01"))
	float CameraRecoilReturnTime = 0.14f;
	
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