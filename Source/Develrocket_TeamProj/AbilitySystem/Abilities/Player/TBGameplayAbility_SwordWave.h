#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/TBGameplayAbility_Projectile.h"
#include "TBGameplayAbility_SwordWave.generated.h"

UCLASS(Blueprintable)
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_SwordWave : public UTBGameplayAbility_Projectile
{
	GENERATED_BODY()

public:
	UTBGameplayAbility_SwordWave();

protected:
	// 진행 방향을 기준으로 한 초승달 회전 각도
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Projectile")
	float ProjectileRoll = 90.0f;

	// 조준 방향을 유지하면서 초승달을 수평으로 회전
	virtual bool GetProjectileLaunchTransform_Implementation(FTransform& OutTransform) const override;
};