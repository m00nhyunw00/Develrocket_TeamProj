#pragma once

#include "Components/TBCharacterMovementComponent.h"
#include "TBMonsterMovementComponent.generated.h"

UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBMonsterMovementComponent : public UTBCharacterMovementComponent
{
	GENERATED_BODY()

public:
	virtual FVector ConsumeInputVector() override;

	void SetChargeMovementActive(bool bActive) { bChargeMovementActive = bActive; }
	bool HasChargeInput() const { return bChargeMovementActive; }
	void SetLeapMovementActive(bool bActive) { bLeapMovementActive = bActive; }
	void SetBoomJumpActive(bool bActive) { bBoomJumpActive = bActive; }
	bool HasActiveSkillMovement() const { return HasChargeInput() || bLeapMovementActive || bBoomJumpActive; }

protected:
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;

private:
	bool bChargeMovementActive = false;
	bool bLeapMovementActive = false;
	bool bBoomJumpActive = false;
};
