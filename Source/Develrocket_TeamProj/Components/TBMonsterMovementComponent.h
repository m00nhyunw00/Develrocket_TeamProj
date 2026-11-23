#pragma once

#include "Components/TBCharacterMovementComponent.h"
#include "TBMonsterMovementComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FTBBoomJumpImpact, const FHitResult&);

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
	FTBBoomJumpImpact OnBoomJumpImpact;

protected:
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void HandleImpact(const FHitResult& Impact, float TimeSlice, const FVector& MoveDelta) override;

private:
	bool bChargeMovementActive = false;
	bool bLeapMovementActive = false;
	bool bBoomJumpActive = false;
};
