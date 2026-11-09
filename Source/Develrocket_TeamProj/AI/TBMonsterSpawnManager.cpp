#include "AI/TBMonsterSpawnManager.h"

#include "Character/Monster/TBMonster.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "Engine/TargetPoint.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "PCG/CityLayoutCoordinator.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogMonsterSpawn, Log, All);

ATBMonsterSpawnManager::ATBMonsterSpawnManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ATBMonsterSpawnManager::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority()) return;

	if (!MonsterClass)
	{
		UE_LOG(LogMonsterSpawn, Warning, TEXT("%s: MonsterClass is not configured."), *GetName());
		return;
	}

	City = Cast<ACityLayoutCoordinator>(UGameplayStatics::GetActorOfClass(this, ACityLayoutCoordinator::StaticClass()));
	if (!City)
	{
		UE_LOG(LogMonsterSpawn, Warning, TEXT("%s: CityLayoutCoordinator was not found."), *GetName());
		return;
	}

	City->OnLocalCityReady.AddDynamic(this, &ThisClass::HandleCityReady);
	UE_LOG(LogMonsterSpawn, Log, TEXT("%s: Waiting for city generation. Class=%s, desired=%d, point tag=%s"), *GetName(), *GetNameSafe(MonsterClass.Get()), DesiredMonsterCount, *PatrolPointTag.ToString());
	if (City->bLocalCityReady) HandleCityReady(City->Layout.Seed, City->Layout.Revision);
}

void ATBMonsterSpawnManager::HandleCityReady(int32 Seed, int32 Revision)
{
	if (!HasAuthority()) return;
	UE_LOG(LogMonsterSpawn, Log, TEXT("%s: City ready. Seed=%d, revision=%d"), *GetName(), Seed, Revision);
	if (!bCityReady)
	{
		bCityReady = true;
		for (int32 Index = 0; Index < DesiredMonsterCount; ++Index) SpawnReadyTimes.Add(0.0);
	}
	GetWorldTimerManager().SetTimer(SpawnTimer, this, &ThisClass::TrySpawn, FMath::Max(0.1f, RetryInterval), true, 0.01f);
}

void ATBMonsterSpawnManager::TrySpawn()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || !City || !City->bLocalCityReady || !MonsterClass || SpawnReadyTimes.IsEmpty()) return;

	const double Now = World->GetTimeSeconds();
	const int32 ReadyIndex = SpawnReadyTimes.IndexOfByPredicate([Now](double ReadyTime) { return ReadyTime <= Now; });
	if (ReadyIndex == INDEX_NONE) return;

	FTransform SpawnTransform;
	if (!FindSpawnTransform(SpawnTransform))
	{
		if (++ConsecutiveFailures % 10 == 0)
		{
			UE_LOG(LogMonsterSpawn, Warning, TEXT("%s: No suitable patrol point found after %d attempts; retrying. Check PCG points, NavMesh and clearance."), *GetName(), ConsecutiveFailures);
		}
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
	ATBMonster* Monster = World->SpawnActor<ATBMonster>(MonsterClass, SpawnTransform, Params);
	if (!Monster)
	{
		if (++ConsecutiveFailures % 10 == 1)
		{
			UE_LOG(LogMonsterSpawn, Warning, TEXT("%s: SpawnActor failed for %s at %s; retrying. Failures=%d"), *GetName(), *GetNameSafe(MonsterClass.Get()), *SpawnTransform.GetLocation().ToString(), ConsecutiveFailures);
		}
		return;
	}
	ConsecutiveFailures = 0;

	LivingMonsters.Add(Monster);
	Monster->OnMonsterDied.AddDynamic(this, &ThisClass::HandleMonsterDied);
	Monster->OnDestroyed.AddDynamic(this, &ThisClass::HandleMonsterDestroyed);
	SpawnReadyTimes.RemoveAtSwap(ReadyIndex);
	UE_LOG(LogMonsterSpawn, Log, TEXT("%s: Spawned %s at %s. Living=%d, pending=%d"), *GetName(), *GetNameSafe(Monster), *Monster->GetActorLocation().ToString(), LivingMonsters.Num(), SpawnReadyTimes.Num());
}

bool ATBMonsterSpawnManager::FindSpawnTransform(FTransform& OutTransform) const
{
	UWorld* World = GetWorld();
	const UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	const ATBMonster* Defaults = MonsterClass ? MonsterClass->GetDefaultObject<ATBMonster>() : nullptr;
	const UCapsuleComponent* Capsule = Defaults ? Defaults->GetCapsuleComponent() : nullptr;
	if (!World || !Nav || !Capsule) return false;

	TArray<AActor*> Points;
	UGameplayStatics::GetAllActorsWithTag(World, PatrolPointTag, Points);
	for (int32 Index = Points.Num() - 1; Index > 0; --Index) Points.Swap(Index, FMath::RandRange(0, Index));

	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector Extent(NavProjectionExtent, NavProjectionExtent, NavProjectionExtent);
	const int32 Attempts = FMath::Min(Points.Num(), FMath::Max(1, MaxCandidatesPerAttempt));
	for (int32 Index = 0; Index < Attempts; ++Index)
	{
		const AActor* Point = Points[Index];
		if (!IsValid(Point) || !Point->IsA<ATargetPoint>()) continue;
		FNavLocation NavLocation;
		if (!Nav->ProjectPointToNavigation(Point->GetActorLocation(), NavLocation, Extent)) continue;
		const FVector Location = NavLocation.Location + FVector(0.0, 0.0, HalfHeight + 2.0f);

		bool bTooClose = false;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APawn* Player = It->IsValid() ? (*It)->GetPawn() : nullptr;
			if (IsValid(Player) && FVector::DistSquared2D(Location, Player->GetActorLocation()) < FMath::Square(MinimumPlayerDistance)) { bTooClose = true; break; }
		}
		if (bTooClose) continue;
		for (TActorIterator<ATBMonster> It(World); It; ++It)
		{
			if (!It->IsDead() && FVector::DistSquared2D(Location, It->GetActorLocation()) < FMath::Square(MinimumMonsterDistance)) { bTooClose = true; break; }
		}
		if (bTooClose) continue;

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MonsterSpawn), false);
		QueryParams.AddIgnoredActor(Point);
		const FCollisionShape Shape = FCollisionShape::MakeCapsule(FMath::Max(1.0f, Radius - 1.0f), FMath::Max(1.0f, HalfHeight - 1.0f));
		if (World->OverlapBlockingTestByProfile(Location, FQuat::Identity, Capsule->GetCollisionProfileName(), Shape, QueryParams)) continue;
		OutTransform = FTransform(FRotator(0.0, FMath::FRandRange(0.0f, 360.0f), 0.0), Location);
		return true;
	}
	return false;
}

void ATBMonsterSpawnManager::HandleMonsterDied(ATBMonster* Monster)
{
	RemoveMonster(Monster);
}

void ATBMonsterSpawnManager::HandleMonsterDestroyed(AActor* Actor)
{
	RemoveMonster(Cast<ATBMonster>(Actor));
}

void ATBMonsterSpawnManager::RemoveMonster(ATBMonster* Monster)
{
	if (!HasAuthority() || !Monster || LivingMonsters.Remove(Monster) == 0) return;
	Monster->OnMonsterDied.RemoveDynamic(this, &ThisClass::HandleMonsterDied);
	Monster->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleMonsterDestroyed);
	SpawnReadyTimes.Add(GetWorld()->GetTimeSeconds() + FMath::Max(0.0f, RespawnDelay));
	UE_LOG(LogMonsterSpawn, Log, TEXT("%s: %s removed from living count; respawn scheduled in %.1f seconds. Living=%d, pending=%d"), *GetName(), *GetNameSafe(Monster), FMath::Max(0.0f, RespawnDelay), LivingMonsters.Num(), SpawnReadyTimes.Num());
}

void ATBMonsterSpawnManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(SpawnTimer);
	if (City) City->OnLocalCityReady.RemoveDynamic(this, &ThisClass::HandleCityReady);
	for (const TWeakObjectPtr<ATBMonster>& Monster : LivingMonsters)
	{
		if (Monster.IsValid())
		{
			Monster->OnMonsterDied.RemoveDynamic(this, &ThisClass::HandleMonsterDied);
			Monster->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleMonsterDestroyed);
		}
	}
	Super::EndPlay(EndPlayReason);
}
