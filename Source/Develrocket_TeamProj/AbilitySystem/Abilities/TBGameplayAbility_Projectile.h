#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/TBGameplayAbility_Attack.h"
#include "TBGameplayAbility_Projectile.generated.h"

class ATBProjectile;

UCLASS(Blueprintable)
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_Projectile : public UTBGameplayAbility_Attack
{
	GENERATED_BODY()

public:
	UTBGameplayAbility_Projectile();

	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	virtual bool CompletesAttackActionDuringActivation() const override
	{
		return true;
	}

protected:
	// 이번 기능에서 발사할 투사체 BP
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Projectile")
	TSubclassOf<ATBProjectile> ProjectileClass;

	// 눈 위치에서 목표 지점을 찾는 조준 검사 거리
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Projectile", meta = (ClampMin = "1.0", Units = "cm"))
	float ProjectileAimTraceDistance = 10000.0f;

	// 눈 위치를 기준으로 한 발사 높이 보정값
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Projectile", meta = (Units = "cm"))
	float ProjectileSpawnHeightOffset = 0.0f;

	// 스킬별 발사 위치·방향 확장 지점
	UFUNCTION(BlueprintNativeEvent, Category = "Combat|Projectile")
	bool GetProjectileLaunchTransform(FTransform& OutTransform) const;

	virtual bool GetProjectileLaunchTransform_Implementation(FTransform& OutTransform) const;

	// 발사 후 행동 제한이 생겨도 비행 중 명중 허용
	virtual bool RequiresUnblockedAttackerOnHit() const override
	{
		return false;
	}

	// 발사 후 몽타주 완료까지 행동 유지 여부
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Recovery")
	bool bWaitForRecovery = false;

	virtual bool ShouldWaitForMontageEnd() const override
	{
		return bWaitForRecovery;
	}

private:
	// 발사 준비 중인 투사체 관리
	TWeakObjectPtr<ATBProjectile> ActiveProjectile;
};
