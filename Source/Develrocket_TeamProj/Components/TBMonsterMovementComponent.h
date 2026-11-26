#pragma once

#include "Components/TBCharacterMovementComponent.h"
#include "AITypes.h"
#include "TBMonsterMovementComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FTBBoomJumpImpact, const FHitResult&);
class UPathFollowingComponent;

UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBMonsterMovementComponent : public UTBCharacterMovementComponent
{
	GENERATED_BODY()

public:
	virtual FVector ConsumeInputVector() override;
	void SteerAroundPathObstacle(UPathFollowingComponent* PathFollowing, FVector& MoveInput);
	void ResetAnasimusEvasion();

	void SetChargeMovementActive(bool bActive) { bChargeMovementActive = bActive; }
	bool HasChargeInput() const { return bChargeMovementActive; }
	void SetLeapMovementActive(bool bActive) { bLeapMovementActive = bActive; }
	void SetBoomJumpActive(bool bActive) { bBoomJumpActive = bActive; }
	bool HasActiveSkillMovement() const { return HasChargeInput() || bLeapMovementActive || bBoomJumpActive; }
	FTBBoomJumpImpact OnBoomJumpImpact;

protected:
	virtual void PhysicsRotation(float DeltaTime) override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void HandleImpact(const FHitResult& Impact, float TimeSlice, const FVector& MoveDelta) override;

private:
	void UpdateAnasimusEvasion(UPathFollowingComponent& PathFollowing, const FVector& Forward,
		double Now, double ProbeDistance, TFunctionRef<bool(const FVector&)> CanSteer);

	FVector PathAvoidanceDirection = FVector::ZeroVector;
	FAIRequestID AvoidanceMoveId;
	double NextPathObstacleProbeTime = 0.0;
	float PreferredAvoidanceSide = 1.0f;
	double AnasimusEvasionStartTime = 0.0;
	double AnasimusEvasionEndTime = 0.0;
	double NextAnasimusEvasionTime = 0.0;
	float AnasimusEvasionTurnAngle = 0.0f;
	FVector AnasimusEvasionStartView = FVector::ZeroVector;
	TWeakObjectPtr<AActor> AnasimusEvasionTarget;
	bool bAnasimusEvadedWhileWatched = false;
	bool bChargeMovementActive = false;
	bool bLeapMovementActive = false;
	bool bBoomJumpActive = false;
};
