#include "AI/TBBaseAIController.h"
#include "AI/TBPatrolPoint.h"
#include "AI/TBPatrolPointSubsystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/NavMovementComponent.h"

#include "EngineUtils.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"

AActor* ATBBaseAIController::SelectNextPatrolPoint(TSubclassOf<AActor> PointClass, float SearchRadius)
{
	APawn* ControlledPawn = GetPawn();
	if (!IsValid(ControlledPawn)) return nullptr;
	if (CurrentPatrolPoint.IsValid() && FVector::DistSquared2D(ControlledPawn->GetActorLocation(), CurrentPatrolPoint->GetActorLocation()) > FMath::Square(300.0f))
	{
		CurrentPatrolPoint.Reset();
		PreviousPatrolPoint.Reset();
	}
	UNavigationSystemV1* Nav = GetWorld() ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()) : nullptr;
	const UNavMovementComponent* Movement = Cast<UNavMovementComponent>(ControlledPawn->GetMovementComponent());
	const ANavigationData* Data = (Nav && Movement) ? Nav->GetNavDataForProps(Movement->GetNavAgentPropertiesRef(), ControlledPawn->GetActorLocation()) : nullptr;
	const UTBPatrolPointSubsystem* Patrols = GetWorld()->GetSubsystem<UTBPatrolPointSubsystem>();
	ATBPatrolPoint* Next = (Patrols && Patrols->bPatrolGraphReady && CurrentPatrolPoint.IsValid()) ? CurrentPatrolPoint->ChooseNextPoint(Data, PreviousPatrolPoint.Get()) : nullptr;
	if (Next && (!Next->IsA(PointClass) || FVector::DistSquared(ControlledPawn->GetActorLocation(), Next->GetActorLocation()) > FMath::Square(SearchRadius))) Next = nullptr;
	if (Next)
	{
		PendingPatrolPoint = Next;
		return Next;
	}
	// Existing patrol Blueprints may not derive from ATBPatrolPoint.
	// Keep the original search usable for those classes instead of discarding its result.
	AActor* FallbackPoint = FindReachablePatrolPoint(PointClass, SearchRadius);
	PendingPatrolPoint = Cast<ATBPatrolPoint>(FallbackPoint);
	return FallbackPoint;
}

FPathFollowingRequestResult ATBBaseAIController::MoveTo(const FAIMoveRequest& MoveRequest, FNavPathSharedPtr* OutPath)
{
	ATBPatrolPoint* Pending = PendingPatrolPoint.Get();
	const bool bPatrolMove = IsValid(Pending) && !MoveRequest.IsMoveToActorRequest() && MoveRequest.GetGoalLocation().Equals(Pending->GetActorLocation(), 1.0f);
	const FPathFollowingRequestResult Result = Super::MoveTo(MoveRequest, OutPath);
	if (bPatrolMove && PendingPatrolPoint.Get() == Pending)
	{
		if (Result.Code == EPathFollowingRequestResult::RequestSuccessful) PatrolRequestID = Result.MoveId;
		else if (Result.Code == EPathFollowingRequestResult::AlreadyAtGoal)
		{
			PreviousPatrolPoint = CurrentPatrolPoint;
			CurrentPatrolPoint = Pending;
			PendingPatrolPoint.Reset();
		}
		else PendingPatrolPoint.Reset();
	}
	return Result;
}

void ATBBaseAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	if (PatrolRequestID.IsValid() && RequestID == PatrolRequestID)
	{
		if (Result.IsSuccess() && PendingPatrolPoint.IsValid())
		{
			PreviousPatrolPoint = CurrentPatrolPoint;
			CurrentPatrolPoint = PendingPatrolPoint;
		}
		else CurrentPatrolPoint.Reset();
		PendingPatrolPoint.Reset();
		PatrolRequestID = FAIRequestID::InvalidRequest;
	}
	Super::OnMoveCompleted(RequestID, Result);
}

void ATBBaseAIController::OnUnPossess()
{
	CurrentPatrolPoint.Reset(); PreviousPatrolPoint.Reset(); PendingPatrolPoint.Reset();
	PatrolRequestID = FAIRequestID::InvalidRequest;
	Super::OnUnPossess();
}

AActor* ATBBaseAIController::FindReachablePatrolPoint(TSubclassOf<AActor> PointClass, float SearchRadius) const
{
	const APawn* ControlledPawn = GetPawn();
	UWorld* World = GetWorld();
	if (!HasAuthority() || !IsValid(ControlledPawn) || !World || !PointClass || SearchRadius <= 0.0f)
	{
		return nullptr;
	}

	const FVector Start = ControlledPawn->GetActorLocation();
	TArray<AActor*> Candidates;
	for (TActorIterator<AActor> It(World, PointClass); It; ++It)
	{
		AActor* Point = *It;
		if (!IsValid(Point) || FVector::DistSquared(Start, Point->GetActorLocation()) > FMath::Square(SearchRadius))
		{
			continue;
		}
		Candidates.Add(Point);
	}
	// 가까운 후보만 남긴 뒤 무작위 순서로 검사하고, 첫 번째 유효한 경로에서 종료한다.
	Candidates.Sort([&Start](const AActor& A, const AActor& B)
	{
		return FVector::DistSquared(Start, A.GetActorLocation()) < FVector::DistSquared(Start, B.GetActorLocation());
	});
	Candidates.SetNum(FMath::Min(Candidates.Num(), FMath::Max(1, MaxPatrolCandidates)));
	for (int32 Index = Candidates.Num() - 1; Index > 0; --Index)
	{
		Candidates.Swap(Index, FMath::RandRange(0, Index));
	}
	for (AActor* Point : Candidates)
	{
		const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(
			World, Start, Point->GetActorLocation(), const_cast<APawn*>(ControlledPawn));
		if (IsValid(Path) && Path->IsValid() && !Path->IsPartial()) return Point;
	}
	return nullptr;
}
