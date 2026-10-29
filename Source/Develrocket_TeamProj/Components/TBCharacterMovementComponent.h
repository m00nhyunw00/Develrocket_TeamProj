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

private:
	bool IsMovementBlockedByState() const;
};