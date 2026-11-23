#include "AI/Tasks/Monster/BTTask_VampireHunterCombatMove.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/Monster/TBMonster.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"

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
	MovementMode = EMovementMode::Chase;
	TimeUntilUpdate = 0.1f;
	return UpdateMovement(OwnerComp) ? EBTNodeResult::InProgress : EBTNodeResult::Failed;
}

void UBTTask_VampireHunterCombatMove::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	TimeUntilUpdate -= DeltaSeconds;
	if (TimeUntilUpdate <= 0.0f)
	{
		TimeUntilUpdate = 0.1f;
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

	const bool bMoveActive = MoveId.IsValid() && Controller->GetCurrentMoveRequestID() == MoveId;
	const bool bTargetMoved = FVector::DistSquared2D(LastTargetLocation, TargetLocation) > FMath::Square(40.0);
	if (!bTargetChanged && bMoveActive && PreviousMode == MovementMode
		&& (MovementMode == EMovementMode::Chase || !bTargetMoved))
	{
		return true;
	}

	if (MovementMode == EMovementMode::Chase)
	{
		FAIMoveRequest Request(Target);
		Request.SetAcceptanceRadius(FMath::Max(5.0f, OrbitStartDistance - 20.0f));
		Request.SetAllowPartialPath(true);
		StartMove(*Controller, Request);
		return true;
	}

	const FVector Radial = Offset.GetSafeNormal2D(UE_SMALL_NUMBER, -Monster->GetActorForwardVector().GetSafeNormal2D());
	const FVector Center(TargetLocation.X, TargetLocation.Y, Origin.Z);
	LastTargetLocation = TargetLocation;
	if (MovementMode == EMovementMode::Retreat)
	{
		const double GoalRadius = RetreatEnd + 35.0;
		const double MinimumRadius = FMath::Min(RetreatEnd, Distance + 20.0);
		for (const float Angle : {0.0f, 30.0f * OrbitSide, -30.0f * OrbitSide})
		{
			const FVector Goal = Center + Radial.RotateAngleAxis(Angle, FVector::UpVector) * GoalRadius;
			if (TryLocalMove(*Controller, Origin, Goal, TargetLocation, MinimumRadius))
			{
				return true;
			}
		}
	}
	else
	{
		const double PreferredRadius = FMath::Clamp<double>(OrbitPreferredDistance, RetreatEnd + 10.0, FMath::Max<double>(RetreatEnd + 10.0, OrbitStartDistance));
		const double GoalRadius = FMath::Max(PreferredRadius, Distance - 35.0);
		for (const float Side : {OrbitSide, -OrbitSide})
		{
			const FVector Goal = Center + Radial.RotateAngleAxis(25.0f * Side, FVector::UpVector) * GoalRadius;
			if (TryLocalMove(*Controller, Origin, Goal, TargetLocation, RetreatEnd))
			{
				OrbitSide = Side;
				return true;
			}
		}
	}

	StopOwnedMove(*Controller);
	return true;
}

bool UBTTask_VampireHunterCombatMove::TryLocalMove(AAIController& Controller, const FVector& Origin, const FVector& Goal,
	const FVector& TargetLocation, double MinimumRadius)
{
	const UNavigationSystemV1* Navigation = UNavigationSystemV1::GetCurrent(Controller.GetWorld());
	const INavAgentInterface* NavAgent = Cast<INavAgentInterface>(Controller.GetPawn());
	const ANavigationData* NavData = Navigation && NavAgent
		? Navigation->GetNavDataForProps(NavAgent->GetNavAgentPropertiesRef(), Origin)
		: nullptr;
	FNavLocation Projected;
	if (!NavData || !Navigation->ProjectPointToNavigation(Goal, Projected, FVector(45.0, 45.0, 160.0), NavData)
		|| FVector::DistSquared2D(Projected.Location, Origin) < FMath::Square(35.0)
		|| FVector::DistSquared2D(Projected.Location, TargetLocation) < FMath::Square(MinimumRadius))
	{
		return false;
	}

	FVector BlockedLocation;
	if (UNavigationSystemV1::NavigationRaycast(&Controller, NavAgent->GetNavAgentLocation(), Projected.Location, BlockedLocation, nullptr, &Controller))
	{
		return false;
	}

	FAIMoveRequest Request(Projected.Location);
	Request.SetAcceptanceRadius(18.0f);
	Request.SetAllowPartialPath(false);
	return StartMove(Controller, Request);
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
