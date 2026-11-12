#include "AI/TBBaseAIController.h"

#include "EngineUtils.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"

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
		const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(
			World, Start, Point->GetActorLocation(), const_cast<APawn*>(ControlledPawn));
		if (IsValid(Path) && Path->IsValid() && !Path->IsPartial())
		{
			Candidates.Add(Point);
		}
	}
	return Candidates.IsEmpty() ? nullptr : Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
}
