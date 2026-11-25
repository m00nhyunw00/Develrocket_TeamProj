#include "Components/TBMonsterMovementComponent.h"

#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationData.h"
#include "NavigationPath.h"

namespace
{
	constexpr double PathObstacleProbeInterval = 0.12;
}

void UTBMonsterMovementComponent::SteerAroundPathObstacle(UPathFollowingComponent* PathFollowing, FVector& MoveInput)
{
	UWorld* World = GetWorld();
	if (!World || !CharacterOwner || !CharacterOwner->HasAuthority() || HasActiveSkillMovement()
		|| !IsMovingOnGround() || IsMovementBlockedByState())
	{
		PathAvoidanceDirection = FVector::ZeroVector;
		NextPathObstacleProbeTime = 0.0;
		return;
	}

	const FVector Forward = MoveInput.GetSafeNormal2D();
	if (Forward.IsNearlyZero()) return;

	const FAIRequestID MoveId = PathFollowing->GetCurrentRequestId();
	if (MoveId != AvoidanceMoveId)
	{
		AvoidanceMoveId = MoveId;
		PathAvoidanceDirection = FVector::ZeroVector;
		NextPathObstacleProbeTime = 0.0;
	}

	const double Now = World->GetTimeSeconds();
	if (Now >= NextPathObstacleProbeTime)
	{
		NextPathObstacleProbeTime = Now + PathObstacleProbeInterval;
		PathAvoidanceDirection = FVector::ZeroVector;

		const FNavPathSharedPtr Path = PathFollowing->GetPath();
		const ANavigationData* NavData = Path.IsValid() ? Path->GetNavigationDataUsed() : nullptr;
		const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
		if (!NavData || !Capsule || !Capsule->IsQueryCollisionEnabled()) return;

		const float Radius = Capsule->GetScaledCapsuleRadius();
		const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		const float BottomClearance = FMath::Clamp(MaxStepHeight * 0.5f, 0.0f, HalfHeight - Radius);
		const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight - BottomClearance * 0.5f);
		const FVector Start = Capsule->GetComponentLocation() + FVector(0.0, 0.0, BottomClearance * 0.5f);
		const FVector Feet = GetFeetLocation();
		const double ProbeDistance = FMath::Max(95.0, static_cast<double>(Radius) * 1.3);
		const double ForwardDistance = FMath::Min(ProbeDistance,
			FVector::Dist2D(Feet, PathFollowing->GetCurrentTargetLocation()));

		FCollisionQueryParams Params(SCENE_QUERY_STAT(MonsterPathObstacle), false, CharacterOwner);
		Params.AddIgnoredActor(PathFollowing->GetMoveGoal());
		const FCollisionResponseParams Responses(Capsule->GetCollisionResponseToChannels());
		const auto IsBlocked = [&](const FVector& Offset)
		{
			return World->SweepTestByChannel(Start, Start + Offset, Capsule->GetComponentQuat(),
				Capsule->GetCollisionObjectType(), Shape, Params, Responses);
		};

		if (ForwardDistance > 1.0 && IsBlocked(Forward * ForwardDistance))
		{
			for (const float Side : {PreferredAvoidanceSide, -PreferredAvoidanceSide})
			{
				for (const float Angle : {45.0f, 75.0f, 90.0f})
				{
					const FVector Direction = Forward.RotateAngleAxis(Angle * Side, FVector::UpVector);
					const FVector Offset = Direction * ProbeDistance;
					FVector NavHit;
					if (!NavData->Raycast(Feet, Feet + Offset, NavHit, NavData->GetDefaultQueryFilter(), CharacterOwner)
						&& !IsBlocked(Offset))
					{
						PathAvoidanceDirection = Direction;
						PreferredAvoidanceSide = Side;
						break;
					}
				}
				if (!PathAvoidanceDirection.IsNearlyZero()) break;
			}
		}
	}

	if (!PathAvoidanceDirection.IsNearlyZero())
	{
		const FVector SteeringInput = PathAvoidanceDirection * MoveInput.Size2D();
		MoveInput.X = SteeringInput.X;
		MoveInput.Y = SteeringInput.Y;
	}
}

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

void UTBMonsterMovementComponent::HandleImpact(const FHitResult& Impact, float TimeSlice, const FVector& MoveDelta)
{
	if (bBoomJumpActive && IsFalling() && Impact.bBlockingHit)
	{
		OnBoomJumpImpact.Broadcast(Impact);
	}
	Super::HandleImpact(Impact, TimeSlice, MoveDelta);
}
