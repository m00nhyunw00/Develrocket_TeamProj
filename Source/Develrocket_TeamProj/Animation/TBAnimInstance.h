#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "TBAnimInstance.generated.h"

// AnimGraph의 로코모션 포즈 선택에 필요한 캐릭터 상태 제공
UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	// 수직 속도를 제외한 지상 이동 속도
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float GroundSpeed = 0.0f;

	// 방향별 Blend Space에 전달할 캐릭터 전방 기준 이동 각도
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float MovementDirection = 0.0f;

	// 점프·낙하 상태 전이에 사용하는 공중 이동 여부
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	bool bIsFalling = false;

	// 앉기 포즈 전환에 사용하는 실제 캐릭터의 앉기 상태
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	bool bIsCrouching = false;
};
