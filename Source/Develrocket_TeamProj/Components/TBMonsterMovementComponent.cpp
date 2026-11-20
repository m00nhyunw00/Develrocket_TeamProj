#include "Components/TBMonsterMovementComponent.h"

#include "GameFramework/Character.h"

FVector UTBMonsterMovementComponent::ConsumeInputVector()
{
	if (HasChargeInput() && CharacterOwner && !IsMovementBlockedByState())
	{
		// 다른 이동 입력 대신 현재 몸체 전방으로 돌진
		Super::ConsumeInputVector();
		AddInputVector(CharacterOwner->GetActorForwardVector().GetSafeNormal2D(), true);
	}

	return Super::ConsumeInputVector();
}

void UTBMonsterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);

	const bool bAlignCharge = HasChargeInput() && !IsMovementBlockedByState();
	const bool bAlignJump = (bLeapMovementActive || bBoomJumpActive) && IsFalling();
	if (CharacterOwner && (bAlignCharge || bAlignJump))
	{
		// 최신 몸체 방향을 적용하고 수직 속도는 유지
		const FVector HorizontalVelocity = CharacterOwner->GetActorForwardVector().GetSafeNormal2D() * Velocity.Size2D();
		Velocity.X = HorizontalVelocity.X;
		Velocity.Y = HorizontalVelocity.Y;
	}
}
