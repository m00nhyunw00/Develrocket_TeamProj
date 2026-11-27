#include "Components/TBMonsterMovementComponent.h"

#include "AIController.h"
#include "Character/Monster/TBMonster.h"
#include "Character/Player/TBPlayer.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Components/TBRotationComponent.h"
#include "Engine/World.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationData.h"
#include "NavigationPath.h"

namespace
{
	constexpr double PathObstacleProbeInterval = 0.12;
	constexpr double AnasimusEvasionMaxDuration = 1.8;
	constexpr double AnasimusEvasionBlendTime = 0.2;
	constexpr double AnasimusEvasionCooldown = 3.0;
	constexpr float AnasimusEvasionTurnDegrees = 60.0f;
	constexpr double AnasimusWatchDistance = 1050.0;
	constexpr double AnasimusWatchAngle = 30.0;
	constexpr double AnasimusFlankAngle = 45.0;
}

void UTBMonsterMovementComponent::SteerAroundPathObstacle(UPathFollowingComponent* PathFollowing, FVector& MoveInput)
{
	UWorld* World = GetWorld();
	if (!World || !CharacterOwner || !CharacterOwner->HasAuthority() || HasActiveSkillMovement()
		|| !IsMovingOnGround() || IsMovementBlockedByState())
	{
		PathAvoidanceDirection = FVector::ZeroVector;
		NextPathObstacleProbeTime = 0.0;
		AnasimusEvasionTurnAngle = 0.0f;
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
		AnasimusEvasionTurnAngle = 0.0f;
	}

	const double Now = World->GetTimeSeconds();
	if (Now >= NextPathObstacleProbeTime)
	{
		NextPathObstacleProbeTime = Now + PathObstacleProbeInterval;
		PathAvoidanceDirection = FVector::ZeroVector;

		const FNavPathSharedPtr Path = PathFollowing->GetPath();
		const ANavigationData* NavData = Path.IsValid() ? Path->GetNavigationDataUsed() : nullptr;
		const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
		if (!NavData || !Capsule || !Capsule->IsQueryCollisionEnabled())
		{
			AnasimusEvasionTurnAngle = 0.0f;
			return;
		}

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
		const auto CanSteer = [&](const FVector& Offset)
		{
			FVector NavHit;
			return !NavData->Raycast(Feet, Feet + Offset, NavHit, NavData->GetDefaultQueryFilter(), CharacterOwner)
				&& !IsBlocked(Offset);
		};

		if (ForwardDistance > 1.0 && IsBlocked(Forward * ForwardDistance))
		{
			AnasimusEvasionTurnAngle = 0.0f;
			for (const float Side : {PreferredAvoidanceSide, -PreferredAvoidanceSide})
			{
				for (const float Angle : {45.0f, 75.0f, 90.0f})
				{
					const FVector Direction = Forward.RotateAngleAxis(Angle * Side, FVector::UpVector);
					const FVector Offset = Direction * ProbeDistance;
					if (CanSteer(Offset))
					{
						PathAvoidanceDirection = Direction;
						PreferredAvoidanceSide = Side;
						break;
					}
				}
				if (!PathAvoidanceDirection.IsNearlyZero()) break;
			}
		}
		else
		{
			UpdateAnasimusEvasion(*PathFollowing, Forward, Now, ProbeDistance, CanSteer);
		}
	}

	FVector SteeringDirection = PathAvoidanceDirection;
	if (SteeringDirection.IsNearlyZero() && AnasimusEvasionTurnAngle != 0.0f && Now < AnasimusEvasionEndTime)
	{
		const double BlendTime = FMath::Min(Now - AnasimusEvasionStartTime, AnasimusEvasionEndTime - Now);
		const double Blend = FMath::SmoothStep(0.0, AnasimusEvasionBlendTime, BlendTime);
		const double Turn = AnasimusEvasionTurnAngle * Blend;
		SteeringDirection = Forward.RotateAngleAxis(Turn, FVector::UpVector);
	}
	if (!SteeringDirection.IsNearlyZero())
	{
		const FVector SteeringInput = SteeringDirection * MoveInput.Size2D();
		MoveInput.X = SteeringInput.X;
		MoveInput.Y = SteeringInput.Y;
	}
}

void UTBMonsterMovementComponent::ResetAnasimusEvasion()
{
	AnasimusEvasionTarget.Reset();
	bAnasimusEvadedWhileWatched = false;
	AnasimusEvasionTurnAngle = 0.0f;
	NextAnasimusEvasionTime = 0.0;
}

void UTBMonsterMovementComponent::PhysicsRotation(float DeltaTime)
{
	const ATBMonster* Monster = Cast<ATBMonster>(CharacterOwner);
	const UTBRotationComponent* AttackRotation = Monster ? Monster->GetAttackRotationComponent() : nullptr;
	if (AttackRotation && AttackRotation->IsRotatingTowardAbilityTarget())
	{
		return;
	}

	const UWorld* World = GetWorld();
	const AActor* Target = AnasimusEvasionTarget.Get();
	if (AnasimusEvasionTurnAngle != 0.0f && World && World->GetTimeSeconds() < AnasimusEvasionEndTime
		&& Target && HasValidData() && CharacterOwner->HasAuthority() && IsMovingOnGround()
		&& !HasActiveSkillMovement() && !IsMovementBlockedByState())
	{
		const AAIController* Controller = Cast<AAIController>(CharacterOwner->GetController());
		const UPathFollowingComponent* PathFollowing = Controller ? Controller->GetPathFollowingComponent() : nullptr;
		const FVector ToTarget = (Target->GetActorLocation() - UpdatedComponent->GetComponentLocation()).GetSafeNormal2D();
		if (PathFollowing && PathFollowing->GetStatus() == EPathFollowingStatus::Moving
			&& PathFollowing->GetCurrentRequestId() == AvoidanceMoveId && PathFollowing->GetMoveGoal() == Target
			&& !ToTarget.IsNearlyZero())
		{
			FRotator Rotation = UpdatedComponent->GetComponentRotation();
			Rotation.Yaw = FMath::FixedTurn(Rotation.Yaw, ToTarget.Rotation().Yaw, GetDeltaRotation(DeltaTime).Yaw);
			MoveUpdatedComponent(FVector::ZeroVector, Rotation, false);
			return;
		}
	}

	Super::PhysicsRotation(DeltaTime);
}

void UTBMonsterMovementComponent::UpdateAnasimusEvasion(UPathFollowingComponent& PathFollowing, const FVector& Forward,
	double Now, double ProbeDistance, TFunctionRef<bool(const FVector&)> CanSteer)
{
	const ATBMonster* Monster = Cast<ATBMonster>(CharacterOwner);
	ATBPlayer* Target = Cast<ATBPlayer>(PathFollowing.GetMoveGoal());
	if (!Monster || Monster->GetMonsterBehavior() != ETBMonsterBehavior::Anasimus || !IsValid(Target) || Target->IsDead())
	{
		AnasimusEvasionTarget.Reset();
		bAnasimusEvadedWhileWatched = false;
		AnasimusEvasionTurnAngle = 0.0f;
		return;
	}
	if (AnasimusEvasionTarget.Get() != Target)
	{
		AnasimusEvasionTarget = Target;
		bAnasimusEvadedWhileWatched = false;
		AnasimusEvasionTurnAngle = 0.0f;
	}

	const FVector Origin = Monster->GetActorLocation();
	const FVector TargetLocation = Target->GetActorLocation();
	const double Distance = FVector::Dist2D(Origin, TargetLocation);
	const FVector ViewLocation = Target->GetPawnViewLocation();
	const FVector ViewDirection = Target->GetBaseAimRotation().Vector();
	const FVector ToMonster = Origin - ViewLocation;
	const FVector HorizontalView = ViewDirection.GetSafeNormal2D();
	const double ViewDot = FVector::DotProduct(ViewDirection, ToMonster.GetSafeNormal());
	const double FlankDot = FMath::Cos(FMath::DegreesToRadians(AnasimusFlankAngle));
	const double MinimumDistance = Monster->GetMeleeAttackRange() + 80.0;
	const FVector ToTarget = (TargetLocation - Origin).GetSafeNormal2D();
	const auto CanApproachOnSide = [&](float Turn)
	{
		const FVector Offset = Forward.RotateAngleAxis(Turn, FVector::UpVector) * ProbeDistance;
		return FVector::DistSquared2D(Origin + Offset, TargetLocation) < FMath::Square(Distance) && CanSteer(Offset);
	};

	if (AnasimusEvasionTurnAngle != 0.0f && Now < AnasimusEvasionEndTime)
	{
		if (Distance <= MinimumDistance || !CanApproachOnSide(AnasimusEvasionTurnAngle))
		{
			AnasimusEvasionTurnAngle = 0.0f;
		}
		else if (FVector::DotProduct(AnasimusEvasionStartView, ToMonster.GetSafeNormal2D()) <= FlankDot)
		{
			// Commit to the chosen side even if the player tracks the monster with the camera.
			AnasimusEvasionEndTime = FMath::Min(AnasimusEvasionEndTime, Now + AnasimusEvasionBlendTime);
		}
		return;
	}

	AnasimusEvasionTurnAngle = 0.0f;
	if (ViewDot < FlankDot)
	{
		bAnasimusEvadedWhileWatched = false;
	}
	if (bAnasimusEvadedWhileWatched || Now < NextAnasimusEvasionTime || Distance > AnasimusWatchDistance
		|| Distance <= MinimumDistance + ProbeDistance
		|| ViewDot < FMath::Cos(FMath::DegreesToRadians(AnasimusWatchAngle))
		|| HorizontalView.IsNearlyZero() || FVector::DotProduct(Forward, ToTarget) < 0.6)
	{
		return;
	}

	FCollisionQueryParams ViewParams(SCENE_QUERY_STAT(AnasimusPlayerView), false, Target);
	ViewParams.AddIgnoredActor(Monster);
	if (GetWorld()->LineTraceTestByChannel(ViewLocation, Origin, ECC_Visibility, ViewParams)) return;

	double BestAlignment = FVector::DotProduct(HorizontalView, ToMonster.GetSafeNormal2D());
	float BestTurn = 0.0f;
	for (const float Turn : {AnasimusEvasionTurnDegrees, -AnasimusEvasionTurnDegrees})
	{
		const FVector Offset = Forward.RotateAngleAxis(Turn, FVector::UpVector) * ProbeDistance;
		const FVector Goal = Origin + Offset;
		const double Alignment = FVector::DotProduct(HorizontalView, (Goal - ViewLocation).GetSafeNormal2D());
		if (Alignment < BestAlignment - UE_KINDA_SMALL_NUMBER && CanApproachOnSide(Turn))
		{
			BestAlignment = Alignment;
			BestTurn = Turn;
		}
	}
	if (BestTurn == 0.0f) return;

	AnasimusEvasionTurnAngle = BestTurn;
	AnasimusEvasionStartView = HorizontalView;
	AnasimusEvasionStartTime = Now;
	AnasimusEvasionEndTime = Now + AnasimusEvasionMaxDuration;
	NextAnasimusEvasionTime = Now + AnasimusEvasionCooldown;
	bAnasimusEvadedWhileWatched = true;
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
