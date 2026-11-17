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
	bPatrolGraphReady = false;
	bBuilding = false;
	BuildIndex = AgentIndex = CandidateIndex = 0;
	BuildCandidates.Reset();
	// Batch point registrations and removals during city generation.
	FTimerManagerTimerParameters TimerParams;
	TimerParams.bLoop = true;
	TimerParams.bMaxOncePerFrame = true;
	TimerParams.FirstDelay = 0.5f;
	World->GetTimerManager().SetTimer(BuildTimer, this, &ThisClass::BuildNextPoint, 0.02f, TimerParams);
}

void UTBPatrolPointSubsystem::BuildNextPoint()
{
	UWorld* World = GetWorld();
	UNavigationSystemV1* Nav = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	if (!Nav) return;
	if (UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World))
	{
		if (bBuilding) RequestRebuild();
		return;
	}
	if (!bBuilding)
	{
		for (const auto& Point : PatrolPoints) if (Point.IsValid()) Point->ClearPatrolLinks();
		bBuilding = true;
	}
	if (BuildIndex >= PatrolPoints.Num())
	{
		World->GetTimerManager().ClearTimer(BuildTimer);
		bBuilding = false;
		bPatrolGraphReady = true;
		OnPatrolGraphReady.Broadcast();
		return;
	}
	ATBPatrolPoint* StartPoint = PatrolPoints[BuildIndex].Get();
	if (!IsValid(StartPoint))
	{
		++BuildIndex; AgentIndex = CandidateIndex = 0; BuildCandidates.Reset();
		return;
	}
	if (BuildCandidates.IsEmpty() && AgentIndex == 0 && CandidateIndex == 0)
	{
		TArray<ATBPatrolPoint*> Candidates;
		for (const auto& Weak : PatrolPoints)
		{
			ATBPatrolPoint* Other = Weak.Get();
			if (IsValid(Other) && Other != StartPoint && Other->GetPatrolZone() == StartPoint->GetPatrolZone()) Candidates.Add(Other);
		}
		const FVector Origin = StartPoint->GetActorLocation();
		Candidates.Sort([&](const ATBPatrolPoint& A, const ATBPatrolPoint& B) { return FVector::DistSquared(Origin, A.GetActorLocation()) < FVector::DistSquared(Origin, B.GetActorLocation()); });
		Candidates.SetNum(FMath::Min(Candidates.Num(), 8));
		for (ATBPatrolPoint* Candidate : Candidates) BuildCandidates.Add(Candidate);
	}
	const TArray<FNavDataConfig>& Agents = Nav->GetSupportedAgents();
	if (BuildCandidates.IsEmpty() || AgentIndex >= Agents.Num())
	{
		++BuildIndex; AgentIndex = CandidateIndex = 0; BuildCandidates.Reset();
		return;
	}
	const FNavDataConfig& Agent = Agents[AgentIndex];
	ATBPatrolPoint* Other = BuildCandidates[CandidateIndex].Get();
	if (++CandidateIndex >= BuildCandidates.Num()) { CandidateIndex = 0; ++AgentIndex; }
	const FVector Origin = StartPoint->GetActorLocation();
	const ANavigationData* Data = Nav->GetNavDataForProps(Agent, Origin);
	if (!Data || !IsValid(Other)) return;
	FNavLocation Start, End;
	if (!Nav->ProjectPointToNavigation(Origin, Start, Agent.DefaultQueryExtent, Data) ||
		!Nav->ProjectPointToNavigation(Other->GetActorLocation(), End, Agent.DefaultQueryExtent, Data)) return;
	// Reject large projections: Move To still receives the original point location.
	constexpr float MaxProjectionDistance = 50.0f;
	if (FVector::DistSquared2D(Origin, Start.Location) > FMath::Square(MaxProjectionDistance) ||
		FVector::DistSquared2D(Other->GetActorLocation(), End.Location) > FMath::Square(MaxProjectionDistance)) return;
	FPathFindingQuery Query(StartPoint, *Data, Start.Location, End.Location);
	const FPathFindingResult Result = Nav->FindPathSync(Agent, Query);
	if (Result.IsSuccessful() && Result.Path.IsValid() && !Result.Path->IsPartial()) StartPoint->AddPatrolLink(Data, Other);
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

