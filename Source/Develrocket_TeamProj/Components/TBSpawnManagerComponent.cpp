#include "Components/TBSpawnManagerComponent.h"

#include "Framework/TBLevelManager.h"
#include "Interface/TBPoolable.h"
#include "Framework/TBSpawnPoolData.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "PCG/CityLayoutCoordinator.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogTBSpawnPool, Log, All);

UTBSpawnManagerComponent::UTBSpawnManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTBSpawnManagerComponent::StartSpawning()
{
	// 스폰 판단은 서버에서만 수행하며 같은 타이머가 중복 시작되지 않도록 합니다.
	PoolOwner = Cast<ATBLevelManager>(GetOwner());
	if (!PoolOwner || !PoolOwner->HasAuthority() || !GetWorld()) return;
	City = Cast<ACityLayoutCoordinator>(UGameplayStatics::GetActorOfClass(this, ACityLayoutCoordinator::StaticClass()));
	if (GetWorld()->GetTimerManager().IsTimerActive(UpdateTimer)) return;
	NextSpawnTimes.Init(0.0, PoolOwner->RuntimeSpawnPools.Num());
	GetWorld()->GetTimerManager().SetTimer(UpdateTimer, this, &ThisClass::UpdatePools, 0.1f, true, 0.01f);
}

void UTBSpawnManagerComponent::UpdatePools()
{
	// 도시가 재생성 중이면 준비될 때까지 새 스폰 시도를 건너뜁니다.
	if (!IsValid(PoolOwner) || !PoolOwner->HasAuthority() || !IsValid(City) || !City->bLocalCityReady) return;
	UWorld* World = GetWorld();
	if (!World) return;
	NextSpawnTimes.SetNum(PoolOwner->RuntimeSpawnPools.Num());
	for (int32 Index = 0; Index < PoolOwner->RuntimeSpawnPools.Num(); ++Index)
	{
		FTBSpawnPoolRuntime& Entry = PoolOwner->RuntimeSpawnPools[Index];
		const FTBSpawnPoolConfig& Config = Entry.Config;
		// 외부에서 파괴된 액터는 풀 목록에서 제거합니다.
		Entry.ActiveActors.RemoveAll([](const TObjectPtr<AActor>& Actor) { return !IsValid(Actor); });
		Entry.AvailableActors.RemoveAll([](const TObjectPtr<AActor>& Actor) { return !IsValid(Actor); });
		if (!Config.ActorClass || Config.PoolSize <= 0 || Config.MaxActiveCount <= 0) continue;
		if (!Config.ActorClass->ImplementsInterface(UTBPoolable::StaticClass()))
		{
			if (NextSpawnTimes[Index] <= World->GetTimeSeconds())
			{
				UE_LOG(LogTBSpawnPool, Warning, TEXT("%s must implement TBPoolable before it can be pooled."), *GetNameSafe(Config.ActorClass.Get()));
				NextSpawnTimes[Index] = World->GetTimeSeconds() + 10.0;
			}
			continue;
		}
		if (Entry.ActiveActors.Num() >= FMath::Min(Config.MaxActiveCount, Config.PoolSize) || World->GetTimeSeconds() < NextSpawnTimes[Index]) continue;
		// 활성 수와 보유 상한을 함께 적용하고 항목마다 다음 시도 시각을 정합니다.
		NextSpawnTimes[Index] = World->GetTimeSeconds() + FMath::Max(0.1f, Config.SpawnInterval);
		FTransform Transform;
		if (!FindSpawnTransform(Config, Transform)) continue;
		AActor* Actor = nullptr;
		// 대기 중인 액터를 우선 재사용하고, 없으면 풀 크기 한도까지 생성합니다.
		if (!Entry.AvailableActors.IsEmpty())
		{
			Actor = Entry.AvailableActors.Pop();
			if (!IsValid(Actor) || !Actor->SetActorLocationAndRotation(Transform.GetLocation(), Transform.Rotator(), false, nullptr, ETeleportType::TeleportPhysics))
			{
				if (IsValid(Actor)) Entry.AvailableActors.Add(Actor);
				continue;
			}
		}
		else if (Entry.ActiveActors.Num() + Entry.AvailableActors.Num() < Config.PoolSize)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
			Actor = World->SpawnActor<AActor>(Config.ActorClass, Transform, Params);
		}
		if (!IsValid(Actor)) continue;
		// 액터별 초기화는 인터페이스에 맡기고 서버에서 상태 동기화를 요청합니다.
		Entry.ActiveActors.Add(Actor);
		Actor->SetActorHiddenInGame(false);
		Actor->SetActorEnableCollision(true);
		Actor->SetActorTickEnabled(true);
		ITBPoolable::Execute_OnPoolActivated(Actor);
		Actor->ForceNetUpdate();
	}
}

bool UTBSpawnManagerComponent::NotifyActorDied(AActor* Actor)
{
	if (!IsValid(Actor) || !Actor->HasAuthority() || !Actor->GetWorld()) return false;
	ATBLevelManager* Manager = Cast<ATBLevelManager>(UGameplayStatics::GetActorOfClass(Actor, ATBLevelManager::StaticClass()));
	UTBSpawnManagerComponent* SpawnManager = Manager ? Manager->FindComponentByClass<UTBSpawnManagerComponent>() : nullptr;
	if (!IsValid(SpawnManager) || !IsValid(SpawnManager->PoolOwner)) return false;
	for (const FTBSpawnPoolRuntime& Pool : SpawnManager->PoolOwner->RuntimeSpawnPools)
	{
		if (!Pool.ActiveActors.Contains(Actor)) continue;
		const TWeakObjectPtr<AActor> WeakActor(Actor);
		if (SpawnManager->PendingReturns.Contains(WeakActor)) return true;
		TWeakObjectPtr<UTBSpawnManagerComponent> WeakManager(SpawnManager);
		FTimerDelegate Delegate = FTimerDelegate::CreateLambda([WeakManager, WeakActor]()
		{
			if (!WeakManager.IsValid()) return;
			WeakManager->PendingReturns.Remove(WeakActor);
			if (WeakActor.IsValid()) WeakManager->ReturnToPool(WeakActor.Get());
		});
		FTimerHandle Handle;
		const float Delay = FMath::Max(0.0f, Pool.Config.ReturnDelay);
		if (Delay <= 0.0f)
		{
			Handle = Actor->GetWorldTimerManager().SetTimerForNextTick(Delegate);
		}
		else
		{
			Actor->GetWorldTimerManager().SetTimer(Handle, Delegate, Delay, false);
		}
		SpawnManager->PendingReturns.Add(WeakActor, Handle);
		return true;
	}
	return false;
}

bool UTBSpawnManagerComponent::ReturnToPool(AActor* Actor)
{
	// 활성 목록에서 찾은 액터만 반환하여 같은 액터가 중복 등록되는 것을 방지합니다.
	if (!IsValid(PoolOwner) || !PoolOwner->HasAuthority() || !IsValid(Actor)) return false;
	if (FTimerHandle* Handle = PendingReturns.Find(TWeakObjectPtr<AActor>(Actor)))
	{
		GetWorld()->GetTimerManager().ClearTimer(*Handle);
		PendingReturns.Remove(TWeakObjectPtr<AActor>(Actor));
	}
	for (FTBSpawnPoolRuntime& Entry : PoolOwner->RuntimeSpawnPools)
	{
		if (!Entry.ActiveActors.Contains(Actor)) continue;
		Entry.ActiveActors.Remove(Actor);
		ITBPoolable::Execute_OnPoolDeactivated(Actor);
		if (!IsValid(Actor)) return true;
		Actor->SetActorEnableCollision(false);
		Actor->SetActorHiddenInGame(true);
		Actor->SetActorTickEnabled(false);
		Actor->ForceNetUpdate();
		Entry.AvailableActors.Add(Actor);
		return true;
	}
	return false;
}

bool UTBSpawnManagerComponent::FindSpawnTransform(const FTBSpawnPoolConfig& Entry, FTransform& OutTransform) const
{
	UWorld* World = GetWorld();
	if (!World || !Entry.ActorClass || !IsValid(PoolOwner)) return false;

	AActor* Floor = nullptr;
	FBox FloorBounds(ForceInit);
	int32 MaxAttempts = 0;
	float TraceHeight = 0.0f;
	const UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!Nav || !PoolOwner->GetSpawnFloorSearchParameters(Floor, FloorBounds, MaxAttempts, TraceHeight)) return false;

	// Native character capsules retain their existing clearance. Other actors use the
	// configurable capsule and collision profile in the spawn entry.
	const AActor* Defaults = Entry.ActorClass->GetDefaultObject<AActor>();
	const UCapsuleComponent* Capsule = Entry.bUseActorCapsule && Defaults ? Defaults->FindComponentByClass<UCapsuleComponent>() : nullptr;
	if (Entry.bUseActorCapsule && !Capsule)
	{
		UE_LOG(LogTBSpawnPool, Warning, TEXT("%s has no default capsule; disable Use Actor Capsule and configure spawn clearance."), *GetNameSafe(Entry.ActorClass.Get()));
		return false;
	}
	const float Radius = Capsule ? Capsule->GetScaledCapsuleRadius() : FMath::Max(1.0f, Entry.SpawnClearanceRadius);
	const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : FMath::Max(Radius, Entry.SpawnClearanceHalfHeight);
	const FName CollisionProfile = Capsule ? Capsule->GetCollisionProfileName() : Entry.SpawnCollisionProfile;
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(TBPoolFloorSpawn), false);

	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		const double X = FMath::FRandRange(FloorBounds.Min.X, FloorBounds.Max.X);
		const double Y = FMath::FRandRange(FloorBounds.Min.Y, FloorBounds.Max.Y);
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, FVector(X, Y, FloorBounds.Max.Z + TraceHeight), FVector(X, Y, FloorBounds.Min.Z - 1000.0), ECC_Visibility, Query) || Hit.GetActor() != Floor) continue;
		FNavLocation NavLocation;
		if (!Nav->ProjectPointToNavigation(Hit.ImpactPoint, NavLocation, FVector(100.0, 100.0, 250.0))) continue;
		if (FVector::DistSquared2D(Hit.ImpactPoint, NavLocation.Location) > FMath::Square(25.0f) || FMath::Abs(Hit.ImpactPoint.Z - NavLocation.Location.Z) > 50.0f) continue;
		const FVector Location = NavLocation.Location + FVector(0.0, 0.0, HalfHeight + 3.0f);
		bool bTooClose = false;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APawn* Player = It->IsValid() ? (*It)->GetPawn() : nullptr;
			if (IsValid(Player) && FVector::DistSquared2D(Location, Player->GetActorLocation()) < FMath::Square(1500.0f)) { bTooClose = true; break; }
		}
		for (const FTBSpawnPoolRuntime& Pool : PoolOwner->RuntimeSpawnPools)
		{
			for (const TObjectPtr<AActor>& Other : Pool.ActiveActors)
			{
				if (IsValid(Other) && !Other->IsHidden() && Other->GetActorEnableCollision()
					&& FVector::DistSquared2D(Location, Other->GetActorLocation()) < FMath::Square(300.0f))
				{
					bTooClose = true;
					break;
				}
			}
			if (bTooClose) break;
		}
		if (bTooClose || World->OverlapAnyTestByProfile(Location, FQuat::Identity, CollisionProfile, Shape, Query)) continue;
		const FVector Direction = FloorBounds.GetCenter() - Location;
		OutTransform = FTransform(FRotator(0.0f, Direction.Rotation().Yaw, 0.0f), Location);
		return true;
	}
	return false;
}

void UTBSpawnManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		for (auto& Pending : PendingReturns) GetWorld()->GetTimerManager().ClearTimer(Pending.Value);
	}
	PendingReturns.Empty();
	// 컴포넌트 종료 후 스폰 검사가 실행되지 않도록 타이머를 해제합니다.
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(UpdateTimer);
	PoolOwner = nullptr;
	City = nullptr;
	Super::EndPlay(EndPlayReason);
}
