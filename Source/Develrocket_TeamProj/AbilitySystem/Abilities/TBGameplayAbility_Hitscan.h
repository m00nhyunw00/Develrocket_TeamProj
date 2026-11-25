#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/TBGameplayAbility_Attack.h"
#include "TBGameplayAbility_Hitscan.generated.h"

UCLASS(Blueprintable)
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_Hitscan
	: public UTBGameplayAbility_Attack
{
	GENERATED_BODY()

public:
	UTBGameplayAbility_Hitscan();

	virtual void ActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

protected:
	// 기본 공격 외의 히트스캔 기능에서 사용할 사거리
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Hitscan",
		meta = (ClampMin = "1.0"))
	float HitscanRange = 10000.0f;

	// 히트스캔 판정 디버그 표시 여부
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Hitscan")
	bool bDrawHitscanTrace = true;

	// 벽·바닥 등 비생명체의 기본 피격 연출 Cue
	UPROPERTY(EditDefaultsOnly, Category = "GAS|GameplayCue",
		meta = (Categories = "GameplayCue"))
	FGameplayTag DefaultImpactCueTag;

	// 한 발당 기본 반동 각도
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Recoil",
		meta = (ClampMin = "0.0"))
	float CameraRecoilPitch = 0.25f;

	// 반동 최고점까지 상승하는 시간
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Recoil",
		meta = (ClampMin = "0.01"))
	float CameraRecoilRiseTime = 0.04f;

	// 반동 최고점에서 복귀하는 시간
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Recoil",
		meta = (ClampMin = "0.01"))
	float CameraRecoilReturnTime = 0.14f;

	// 몽타주 종료를 기다리지 않고 발사 처리 후 어빌리티 종료
	virtual bool ShouldWaitForMontageEnd() const override
	{
		return false;
	}

	// 트레이스 끝점 결정 후 공격 연출을 한 번만 실행
	virtual bool ShouldExecuteAttackVisualOnCommit() const override
	{
		return false;
	}

private:
	// 명중 대상 검증 및 피해 적용 요청
	void HandleHitscanHit(const FHitResult& HitResult);
};
