#include "AI/TBPatrolPointSubsystem.h"
#include "AI/TBPatrolPoint.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "NavigationPath.h"
#include "TimerManager.h"
#include "Engine/World.h"

void UTBPatrolPointSubsystem::Deinitialize()
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(BuildTimer);
	Super::Deinitialize();
}

void UTBPatrolPointSubsystem::RequestRebuild()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client) return;
	BuildIndex = 0;
	bBuilding = false;
	for (const auto& Point : PatrolPoints) if (Point.IsValid()) Point->ClearPatrolLinks();
	World->GetTimerManager().SetTimer(BuildTimer, this, &ThisClass::BuildNextPoint, 0.02f, true, 0.5f);
}

void UTBPatrolPointSubsystem::BuildNextPoint()
{
	UWorld* World = GetWorld();
	UNavigationSystemV1* Nav = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	if (!Nav || UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World)) return;
	if (BuildIndex >= PatrolPoints.Num())
	{
		World->GetTimerManager().ClearTimer(BuildTimer);
		bBuilding = false;
		return;
	}
	bBuilding = true;
	ATBPatrolPoint* StartPoint = PatrolPoints[BuildIndex++].Get();
	if (!IsValid(StartPoint)) return;
	TArray<ATBPatrolPoint*> Candidates;
	for (const auto& Weak : PatrolPoints)
	{
		ATBPatrolPoint* Other = Weak.Get();
		if (IsValid(Other) && Other != StartPoint && Other->GetPatrolZone() == StartPoint->GetPatrolZone()) Candidates.Add(Other);
	}
	const FVector Origin = StartPoint->GetActorLocation();
	Candidates.Sort([&](const ATBPatrolPoint& A, const ATBPatrolPoint& B) { return FVector::DistSquared(Origin, A.GetActorLocation()) < FVector::DistSquared(Origin, B.GetActorLocation()); });
	Candidates.SetNum(FMath::Min(Candidates.Num(), 8));
	for (const FNavDataConfig& Agent : Nav->GetSupportedAgents())
	{
		const ANavigationData* Data = Nav->GetNavDataForProps(Agent, Origin);
		if (!Data) continue;
		FNavLocation Start;
		if (!Nav->ProjectPointToNavigation(Origin, Start, Agent.DefaultQueryExtent, Data)) continue;
		for (ATBPatrolPoint* Other : Candidates)
		{
			FNavLocation End;
			if (!Nav->ProjectPointToNavigation(Other->GetActorLocation(), End, Agent.DefaultQueryExtent, Data)) continue;
			FPathFindingQuery Query(StartPoint, *Data, Start.Location, End.Location);
			const FPathFindingResult Result = Nav->FindPathSync(Agent, Query);
			if (Result.IsSuccessful() && Result.Path.IsValid() && !Result.Path->IsPartial()) StartPoint->AddPatrolLink(Data, Other);
		}
	}
}

void UTBPatrolPointSubsystem::RegisterPatrolPoint(ATBPatrolPoint* Point)
{
	if (!Point)
	{
		return;
	}
	
	PatrolPoints.Add(Point);
	RequestRebuild();
}

void UTBPatrolPointSubsystem::UnregisterPatrolPoint(ATBPatrolPoint* Point)
{
	PatrolPoints.Remove(Point);
	RequestRebuild();
}

ATBPatrolPoint* UTBPatrolPointSubsystem::FindPatrolPoint(const FVector& FromLocation, const FGameplayTag& Zone,
	float MaxDistance, const ATBPatrolPoint* ExcludePoint) const
{
	TArray<ATBPatrolPoint*> Candidates;
	
	const float MaxDistanceSq = MaxDistance * MaxDistance;
	
	for (const TWeakObjectPtr<ATBPatrolPoint>& WeakPoint : PatrolPoints)
	{
		ATBPatrolPoint* Point = WeakPoint.Get();
		
		if (!Point || Point == ExcludePoint)
		{
			continue;
		}
		
		if (Zone.IsValid() && !Point->GetPatrolZone().MatchesTagExact(Zone))
		{
			continue;
		}
		
		if (FVector::DistSquared(FromLocation, Point->GetActorLocation())>MaxDistanceSq)
		{
			continue;
		}
		
		Candidates.Add(Point);
	}
	
	if (Candidates.IsEmpty())
	{
		return nullptr;
	}
	
	return Candidates[FMath::RandRange(0,Candidates.Num()-1)];
}

