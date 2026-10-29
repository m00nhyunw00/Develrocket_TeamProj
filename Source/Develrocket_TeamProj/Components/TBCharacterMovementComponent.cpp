#include "TBCharacterMovementComponent.h"

#include "Character/TBCharacterBase.h"

bool UTBCharacterMovementComponent::IsMovementBlockedByState() const
{
	const ATBCharacterBase* Character = Cast<ATBCharacterBase>(CharacterOwner);

	return Character && Character->IsMovementBlocked();
}

float UTBCharacterMovementComponent::GetMaxSpeed() const
{
	if (IsMovementBlockedByState())
	{
		return 0.0f;
	}

	return Super::GetMaxSpeed();
}

float UTBCharacterMovementComponent::GetMaxAcceleration() const
{
	if (IsMovementBlockedByState())
	{
		return 0.0f;
	}

	return Super::GetMaxAcceleration();
}

bool UTBCharacterMovementComponent::CanAttemptJump() const
{
	return !IsMovementBlockedByState() && Super::CanAttemptJump();
}