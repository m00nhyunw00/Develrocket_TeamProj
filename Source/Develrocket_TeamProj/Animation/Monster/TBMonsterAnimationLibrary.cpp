#include "TBMonsterAnimationLibrary.h"

float UTBMonsterAnimationLibrary::CalculateLocomotionPlayRate(float GroundSpeed, float MinMovingSampleSpeed,
	float MaxMovingSampleSpeed)
{
	if (GroundSpeed <= 0.0f || MinMovingSampleSpeed <= 0.0f || MaxMovingSampleSpeed < MinMovingSampleSpeed)
	{
		return 1.0f;
	}

	// Inside the moving sample range, the Blend Space already blends gaits authored for this speed.
	const float ReferenceSpeed = FMath::Clamp(GroundSpeed, MinMovingSampleSpeed, MaxMovingSampleSpeed);
	const float MovingRate = FMath::Clamp(GroundSpeed / ReferenceSpeed, 0.65f, 1.5f);
	const float MovingWeight = FMath::Min(GroundSpeed / MinMovingSampleSpeed, 1.0f);
	return FMath::Lerp(1.0f, MovingRate, MovingWeight);
}
