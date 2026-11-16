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
