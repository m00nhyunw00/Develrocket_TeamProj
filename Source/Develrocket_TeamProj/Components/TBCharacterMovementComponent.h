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

	virtual FVector ConsumeInputVector() override;

	void SetChargeInputDirection(const FVector& Direction) { ChargeInputDirection = Direction; }
	bool HasChargeInput() const { return !ChargeInputDirection.IsNearlyZero(); }
	void SetLeapMovementActive(bool bActive) { bLeapMovementActive = bActive; }
	bool HasActiveSkillMovement() const { return HasChargeInput() || bLeapMovementActive; }

private:
	bool IsMovementBlockedByState() const;
	FVector ChargeInputDirection = FVector::ZeroVector;
	bool bLeapMovementActive = false;
};
