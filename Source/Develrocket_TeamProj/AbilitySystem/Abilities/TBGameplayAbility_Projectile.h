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

	virtual void ActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	virtual bool CompletesAttackActionDuringActivation() const override
	{
		return true;
	}

protected:
	// 이번 기능에서 발사할 투사체 BP
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Projectile")
	TSubclassOf<ATBProjectile> ProjectileClass;

	// 스킬별 발사 위치·방향 확장 지점
	UFUNCTION(BlueprintNativeEvent, Category = "Combat|Projectile")
	bool GetProjectileLaunchTransform(FTransform& OutTransform) const;

	virtual bool GetProjectileLaunchTransform_Implementation(FTransform& OutTransform) const;

	// 발사 후 행동 제한이 생겨도 비행 중 명중 허용
	virtual bool RequiresUnblockedAttackerOnHit() const override
	{
		return false;
	}

	// 몽타주 종료와 투사체 수명 분리
	virtual bool ShouldWaitForMontageEnd() const override
	{
		return false;
	}

private:
	void HandleProjectileHit(const FHitResult& HitResult);
	void HandleProjectileFinished(bool bWasLaunched);

	TWeakObjectPtr<ATBProjectile> ActiveProjectile;

	FDelegateHandle HitDelegateHandle;
	FDelegateHandle FinishedDelegateHandle;

	// 이 실행의 투사체로 이미 피해를 적용한 대상
	TSet<TWeakObjectPtr<AActor>> DamagedActors;
};