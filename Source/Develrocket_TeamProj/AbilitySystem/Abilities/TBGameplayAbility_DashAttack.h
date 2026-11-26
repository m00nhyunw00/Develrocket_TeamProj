#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/TBGameplayAbility_Melee.h"
#include "TBGameplayAbility_DashAttack.generated.h"

class ATBAmanai;

// 시선 방향으로 돌진하며 기존 근접 트레이스로 피해를 주는 이동기
UCLASS(Blueprintable)
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_DashAttack : public UTBGameplayAbility_Melee
{
	GENERATED_BODY()

public:
	UTBGameplayAbility_DashAttack();

	// 3인칭 Melee Trace 시작 프레임에 호출
	void StartDashFromMeleeTrace(ATBAmanai& Character);

	virtual void EndAbility(FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	virtual void OnActionCommitted() override;

	// 장애물이 없을 때 이동할 거리(cm)
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Dash", meta = (ClampMin = "1.0", Units = "cm"))
	float DashDistance = 750.0f;

	// 전체 거리를 이동하는 데 걸리는 시간
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Dash", meta = (ClampMin = "0.01", Units = "s"))
	float DashDuration = 0.25f;

private:
	// 발동 순간의 시선 방향. 이번 돌진을 시작하면 소비한다.
	FVector PendingDashDirection = FVector::ZeroVector;

	// 돌진 종료 또는 어빌리티 취소 시 복구할 캐릭터
	TWeakObjectPtr<ATBAmanai> DashingCharacter;

	UFUNCTION()
	void FinishDashTravel();
};