#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/TBGameplayAbility_Action.h"
#include "TBGameplayAbility_Roll.generated.h"

class ACharacter;

// 이동 입력 방향으로 몸을 돌려 앞구르기하는 이동기
// 카메라·1인칭 메시는 컨트롤러 회전을 따르므로 시야는 유지된다
UCLASS(Blueprintable)
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_Roll : public UTBGameplayAbility_Action
{
	GENERATED_BODY()

public:
	UTBGameplayAbility_Roll();

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	virtual void OnActionCommitted() override;

	// 장애물이 없을 때 이동할 거리(cm)
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Roll", meta = (ClampMin = "1.0", Units = "cm"))
	float RollDistance = 500.0f;

	// 전체 거리를 이동하는 데 걸리는 시간
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Roll", meta = (ClampMin = "0.01", Units = "s"))
	float RollDuration = 0.25f;

private:
	TWeakObjectPtr<ACharacter> RollingCharacter;
	
	bool bSavedUseControllerRotationYaw = true;
	
	void RestoreBodyRotation();
};