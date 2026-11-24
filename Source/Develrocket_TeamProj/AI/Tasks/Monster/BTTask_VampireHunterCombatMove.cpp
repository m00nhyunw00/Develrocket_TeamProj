#include "AI/Tasks/Monster/BTTask_VampireHunterCombatMove.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/Monster/TBMonster.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationData.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"

namespace
{
	constexpr float MoveUpdateInterval = 0.1f;
	constexpr double OrbitHandoffDistance = 60.0;
	constexpr double TargetReplanDistance = 40.0;
	constexpr float LocalMoveAcceptanceRadius = 18.0f;
}

UBTTask_VampireHunterCombatMove::UBTTask_VampireHunterCombatMove()
{
	NodeName = TEXT("Hunter Combat Movement");
	bCreateNodeInstance = true;
	INIT_TASK_NODE_NOTIFY_FLAGS();
	TargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(ThisClass, TargetActorKey), AActor::StaticClass());
	TargetActorKey.SelectedKeyName = TEXT("TargetActor");
}

void UBTTask_VampireHunterCombatMove::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);
	if (const UBlackboardData* Blackboard = GetBlackboardAsset())
	{
		TargetActorKey.ResolveSelectedKey(*Blackboard);
	}
}

EBTNodeResult::Type UBTTask_VampireHunterCombatMove::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* Controller = OwnerComp.GetAIOwner();
	ATBMonster* Monster = Controller ? Cast<ATBMonster>(Controller->GetPawn()) : nullptr;
	UCharacterMovementComponent* Movement = Monster ? Monster->GetCharacterMovement() : nullptr;
	if (!Movement || !TargetActorKey.IsSet())
	{
		return EBTNodeResult::Failed;
	}

	ActiveMonster = Monster;
	PreviousFocusActor = Controller->GetFocusActorForPriority(EAIFocusPriority::Gameplay);
	PreviousFocalPoint = Controller->GetFocalPointForPriority(EAIFocusPriority::Gameplay);
	bPreviousOrientToMovement = Movement->bOrientRotationToMovement;
	bPreviousUseControllerDesiredRotation = Movement->bUseControllerDesiredRotation;
	bPreviousUseControllerYaw = Monster->bUseControllerRotationYaw;
	Movement->bOrientRotationToMovement = false;
	Movement->bUseControllerDesiredRotation = true;
	Monster->bUseControllerRotationYaw = false;

	OrbitSide = FMath::RandBool() ? 1.0f : -1.0f;
	OrbitLegsUntilSideChoice = FMath::RandRange(2, 4);
	MovementMode = EMovementMode::Chase;
	TimeUntilUpdate = MoveUpdateInterval;
	return UpdateMovement(OwnerComp) ? EBTNodeResult::InProgress : EBTNodeResult::Failed;
}

void UBTTask_VampireHunterCombatMove::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	TimeUntilUpdate -= DeltaSeconds;
	const ATBMonster* Monster = ActiveMonster.Get();
	const bool bReadyForOrbitHandoff = Monster && FAISystem::IsValidLocation(ActiveOrbitGoal)
		&& FAISystem::IsValidLocation(NextOrbitGoal)
		&& FVector::DistSquared2D(Monster->GetActorLocation(), ActiveOrbitGoal) <= FMath::Square(OrbitHandoffDistance);
	if (TimeUntilUpdate <= 0.0f || bReadyForOrbitHandoff)
	{
		TimeUntilUpdate = MoveUpdateInterval;
		if (!UpdateMovement(OwnerComp))
		{
			FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		}
	}
}

bool UBTTask_VampireHunterCombatMove::UpdateMovement(UBehaviorTreeComponent& OwnerComp)
{
	AAIController* Controller = OwnerComp.GetAIOwner();
	ATBMonster* Monster = ActiveMonster.Get();
	const UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AActor* Target = Blackboard ? Cast<AActor>(Blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName)) : nullptr;
	if (!Controller || !Monster || Controller->GetPawn() != Monster || Monster->IsDead() || !IsValid(Target) || Target == Monster)
	{
		return false;
	}

	const bool bTargetChanged = FocusTarget.Get() != Target;
	if (bTargetChanged)
	{
		FocusTarget = Target;
		Controller->SetFocus(Target);
		ActiveOrbitGoal = NextOrbitGoal = FAISystem::InvalidLocation;
	}
	if (Monster->IsMovementBlocked())
	{
		StopOwnedMove(*Controller);
		return true;
	}

	const FVector Origin = Monster->GetActorLocation();
	const FVector TargetLocation = Target->GetActorLocation();
	const FVector Offset = Origin - TargetLocation;
	const double Distance = Offset.Size2D();
	const double RetreatStart = FMath::Min(RetreatStartDistance, OrbitStartDistance);
	const double RetreatEnd = FMath::Clamp<double>(RetreatEndDistance, RetreatStart, OrbitStartDistance);
	const EMovementMode PreviousMode = MovementMode;
	if (Distance < RetreatStart || (PreviousMode == EMovementMode::Retreat && Distance < RetreatEnd))
	{
		MovementMode = EMovementMode::Retreat;
	}
	else if (Distance > OrbitStartDistance + (PreviousMode == EMovementMode::Chase ? 0.0 : 30.0))
	{
		MovementMode = EMovementMode::Chase;
	}
	else
	{
		MovementMode = EMovementMode::Orbit;
	}

	const UPathFollowingComponent* PathFollowing = Controller->GetPathFollowingComponent();
	const bool bMoveActive = PathFollowing && PathFollowing->GetStatus() != EPathFollowingStatus::Idle
		&& MoveId.IsValid() && PathFollowing->GetCurrentRequestId() == MoveId;
	if (MovementMode == EMovementMode::Orbit)
	{
		const bool bFollowingOrbit = bMoveActive && ActiveMoveMode == EMovementMode::Orbit
			&& FAISystem::IsValidLocation(ActiveOrbitGoal);
		if (FVector::DistSquared2D(OrbitPlanTargetLocation, TargetLocation) > FMath::Square(TargetReplanDistance))
		{
			NextOrbitGoal = FAISystem::InvalidLocation;
		}
		if (!FAISystem::IsValidLocation(NextOrbitGoal))
		{
			FindOrbitGoal(*Controller, bFollowingOrbit ? ActiveOrbitGoal : Origin, TargetLocation, RetreatEnd);
		}
		if (bFollowingOrbit && FVector::DistSquared2D(Origin, ActiveOrbitGoal) > FMath::Square(OrbitHandoffDistance))
		{
			return true;
		}
		if (FAISystem::IsValidLocation(NextOrbitGoal))
		{
			if (StartLocalMove(*Controller, NextOrbitGoal))
			{
				ActiveOrbitGoal = NextOrbitGoal;
				const bool bSideChanged = OrbitSide != NextOrbitSide;
				OrbitSide = NextOrbitSide;
				OrbitLegsUntilSideChoice = bSideChanged || OrbitLegsUntilSideChoice <= 0
					? FMath::RandRange(2, 4) : OrbitLegsUntilSideChoice - 1;
			}
			NextOrbitGoal = FAISystem::InvalidLocation;
		}
		return true;
	}

	ActiveOrbitGoal = NextOrbitGoal = FAISystem::InvalidLocation;
	if (MovementMode == EMovementMode::Chase)
	{
		if (bMoveActive && ActiveMoveMode == EMovementMode::Chase && PathFollowing->GetMoveGoal() == Target)
		{
			return true;
		}
		FAIMoveRequest Request(Target);
		Request.SetAcceptanceRadius(FMath::Max(5.0f, OrbitStartDistance - 20.0f));
		Request.SetAllowPartialPath(true);
		StartMove(*Controller, Request);
		return true;
	}

	const bool bTargetMoved = FVector::DistSquared2D(LastTargetLocation, TargetLocation) > FMath::Square(TargetReplanDistance);
	if (!bTargetChanged && bMoveActive && ActiveMoveMode == EMovementMode::Retreat && !bTargetMoved)
	{
		return true;
	}

	const FVector Radial = Offset.GetSafeNormal2D(UE_SMALL_NUMBER, -Monster->GetActorForwardVector().GetSafeNormal2D());
	const FVector Center(TargetLocation.X, TargetLocation.Y, Origin.Z);
	const double GoalRadius = RetreatEnd + 35.0;
	const double MinimumRadius = FMath::Min(RetreatEnd, Distance + 20.0);
	for (const float Angle : {0.0f, 30.0f * OrbitSide, -30.0f * OrbitSide})
	{
		const FVector Goal = Center + Radial.RotateAngleAxis(Angle, FVector::UpVector) * GoalRadius;
		FVector ProjectedGoal;
		if (FindLocalGoal(*Controller, Origin, Goal, TargetLocation, MinimumRadius, ProjectedGoal)
			&& StartLocalMove(*Controller, ProjectedGoal))
		{
			LastTargetLocation = TargetLocation;
			return true;
		}
	}
	if (ActiveMoveMode != EMovementMode::Retreat)
	{
		StopOwnedMove(*Controller);
	}
	return true;
}

bool UBTTask_VampireHunterCombatMove::FindOrbitGoal(AAIController& Controller, const FVector& From,
	const FVector& TargetLocation, double MinimumRadius)
{
	const FVector Offset = From - TargetLocation;
	const FVector Radial = Offset.GetSafeNormal2D(UE_SMALL_NUMBER, -Controller.GetPawn()->GetActorForwardVector().GetSafeNormal2D());
	const FVector Center(TargetLocation.X, TargetLocation.Y, From.Z);
	const double MinimumPreferredRadius = MinimumRadius + 10.0;
	const double PreferredRadius = FMath::Clamp<double>(OrbitPreferredDistance, MinimumPreferredRadius,
		FMath::Max<double>(MinimumPreferredRadius, OrbitStartDistance));
	const double GoalRadius = FMath::Max(PreferredRadius, Offset.Size2D() - 35.0);
	const float PreferredSide = OrbitLegsUntilSideChoice <= 0 && FMath::RandBool() ? -OrbitSide : OrbitSide;
	const float OrbitAngle = FMath::FRandRange(28.0f, 42.0f);
	for (const float Side : {PreferredSide, -PreferredSide})
	{
		const FVector Goal = Center + Radial.RotateAngleAxis(OrbitAngle * Side, FVector::UpVector) * GoalRadius;
		if (FindLocalGoal(Controller, From, Goal, TargetLocation, MinimumRadius, NextOrbitGoal))
		{
			NextOrbitSide = Side;
			OrbitPlanTargetLocation = TargetLocation;
			return true;
		}
	}
	return false;
}

bool UBTTask_VampireHunterCombatMove::FindLocalGoal(AAIController& Controller, const FVector& From, const FVector& Goal,
	const FVector& TargetLocation, double MinimumRadius, FVector& OutGoal) const
{
	const UNavigationSystemV1* Navigation = UNavigationSystemV1::GetCurrent(Controller.GetWorld());
	const INavAgentInterface* NavAgent = Cast<INavAgentInterface>(Controller.GetPawn());
	const ANavigationData* NavData = Navigation && NavAgent
		? Navigation->GetNavDataForProps(NavAgent->GetNavAgentPropertiesRef(), NavAgent->GetNavAgentLocation())
		: nullptr;
	FNavLocation Projected;
	FNavLocation ProjectedStart;
	if (!NavData || !Navigation->ProjectPointToNavigation(Goal, Projected, FVector(45.0, 45.0, 160.0), NavData)
		|| FVector::DistSquared2D(Projected.Location, From) < FMath::Square(35.0)
		|| FVector::DistSquared2D(Projected.Location, TargetLocation) < FMath::Square(MinimumRadius)
		|| !Navigation->ProjectPointToNavigation(From, ProjectedStart, FVector(45.0, 45.0, 160.0), NavData))
	{
		return false;
	}

	FVector BlockedLocation;
	if (UNavigationSystemV1::NavigationRaycast(&Controller, ProjectedStart.Location, Projected.Location, BlockedLocation, nullptr, &Controller))
	{
		return false;
	}

	OutGoal = Projected.Location;
	return true;
}

bool UBTTask_VampireHunterCombatMove::StartLocalMove(AAIController& Controller, const FVector& Goal)
{
	if (FVector::DistSquared2D(Controller.GetNavAgentLocation(), Goal) < FMath::Square(35.0))
	{
		return false;
	}

	FAIMoveRequest Request(Goal);
	Request.SetAcceptanceRadius(LocalMoveAcceptanceRadius);
	Request.SetAllowPartialPath(false);
	Request.SetCanStrafe(true);
	Request.SetUsePathfinding(true);
	Request.SetReachTestIncludesAgentRadius(false);
	Request.SetReachTestIncludesGoalRadius(false);

	FPathFindingQuery Query;
	FNavPathSharedPtr Path;
	if (Controller.BuildPathfindingQuery(Request, Query))
	{
		Controller.FindPathForMoveRequest(Request, Query, Path);
	}
	if (!Path.IsValid() || !Path->IsValid() || Path->IsPartial())
	{
		return false;
	}

	// Submit only a complete path so a failed candidate cannot cancel the active move.
	const FAIRequestID NewMoveId = Controller.RequestMove(Request, Path);
	if (!NewMoveId.IsValid())
	{
		return false;
	}

	Controller.bAllowStrafe = true;
	MoveId = NewMoveId;
	ActiveMoveMode = MovementMode;
	return true;
}

bool UBTTask_VampireHunterCombatMove::StartMove(AAIController& Controller, FAIMoveRequest& Request)
{
	Request.SetCanStrafe(true);
	Request.SetUsePathfinding(true);
	Request.SetReachTestIncludesAgentRadius(false);
	Request.SetReachTestIncludesGoalRadius(false);
	const FPathFollowingRequestResult Result = Controller.MoveTo(Request);
	if (Result.Code == EPathFollowingRequestResult::RequestSuccessful)
	{
		MoveId = Result.MoveId;
		ActiveMoveMode = MovementMode;
		return true;
	}
	return false;
}

void UBTTask_VampireHunterCombatMove::StopOwnedMove(AAIController& Controller)
{
	if (MoveId.IsValid() && Controller.GetCurrentMoveRequestID() == MoveId)
	{
		Controller.StopMovement();
	}
	MoveId = FAIRequestID::InvalidRequest;
	ActiveOrbitGoal = NextOrbitGoal = FAISystem::InvalidLocation;
}

void UBTTask_VampireHunterCombatMove::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	if (ATBMonster* Monster = ActiveMonster.Get())
	{
		UCharacterMovementComponent* Movement = Monster->GetCharacterMovement();
		Movement->bOrientRotationToMovement = bPreviousOrientToMovement;
		Movement->bUseControllerDesiredRotation = bPreviousUseControllerDesiredRotation;
		Monster->bUseControllerRotationYaw = bPreviousUseControllerYaw;
	}
	if (AAIController* Controller = OwnerComp.GetAIOwner())
	{
		StopOwnedMove(*Controller);
		if (!FocusTarget.IsExplicitlyNull() && Controller->GetFocusActorForPriority(EAIFocusPriority::Gameplay) == FocusTarget.Get())
		{
			if (AActor* Actor = PreviousFocusActor.Get())
			{
				Controller->SetFocus(Actor);
			}
			else if (PreviousFocusActor.IsExplicitlyNull() && FAISystem::IsValidLocation(PreviousFocalPoint))
			{
				Controller->SetFocalPoint(PreviousFocalPoint);
			}
			else
			{
				Controller->ClearFocus(EAIFocusPriority::Gameplay);
			}
		}
	}
	ActiveMonster.Reset();
	FocusTarget.Reset();
	PreviousFocusActor.Reset();
}
