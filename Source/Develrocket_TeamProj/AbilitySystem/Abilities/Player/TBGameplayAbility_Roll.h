#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/TBGameplayAbility_Action.h"
#include "TBGameplayAbility_Roll.generated.h"

// 캐릭터의 수평 정면으로 일정 거리를 구르는 이동기
UCLASS(Blueprintable)
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_Roll : public UTBGameplayAbility_Action
{
	GENERATED_BODY()

public:
	UTBGameplayAbility_Roll();

protected:
	virtual void OnActionCommitted() override;

	// 장애물이 없을 때 이동할 거리(cm)
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Roll", meta = (ClampMin = "1.0", Units = "cm"))
	float RollDistance = 500.0f;

	// 전체 거리를 이동하는 데 걸리는 시간
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Roll", meta = (ClampMin = "0.01", Units = "s"))
	float RollDuration = 0.25f;
};