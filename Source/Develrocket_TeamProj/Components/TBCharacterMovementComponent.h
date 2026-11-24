#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TBCharacterMovementComponent.generated.h"

UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	virtual float GetMaxSpeed() const override;

	virtual float GetMaxAcceleration() const override;

	virtual bool CanAttemptJump() const override;


protected:
	bool IsMovementBlockedByState() const;
	
	// 조준 중 최대 이동 속도 배율
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Aim", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AimMovementSpeedMultiplier = 0.3f;
};
