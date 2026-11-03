#include "AI/TBPatrolPointSubsystem.h"
#include "AI/TBPatrolPoint.h"

void UTBPatrolPointSubsystem::RegisterPatrolPoint(ATBPatrolPoint* Point)
{
	if (!Point)
	{
		return;
	}
	
	PatrolPoints.Add(Point);
}

void UTBPatrolPointSubsystem::UnregisterPatrolPoint(ATBPatrolPoint* Point)
{
	PatrolPoints.Remove(Point);
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

