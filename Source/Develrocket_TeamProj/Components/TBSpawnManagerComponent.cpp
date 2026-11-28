#include "Components/TBSpawnManagerComponent.h"
#include "Components/TBPoolStateComponent.h"
#include "Character/Monster/TBMonster.h"

#include "Framework/TBLevelManager.h"
#include "Interface/TBPoolInterface.h"
#include "Framework/TBSpawnPoolData.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "PCG/CityLayoutCoordinator.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogTBSpawnPool, Log, All);

namespace
{
	struct FSpawnPlayerView
	{
		FVector PawnLocation;
		FVector Eye;
		FVector Forward;
		FVector Right;
	};

	bool IsSpawnVisible(const UWorld& World, const FSpawnPlayerView& View, const FVector& Location,
		float Radius, float HalfHeight, const FCollisionQueryParams& Query)
	{
		// 서버의 시야각 근사에 화면 가장자리와 시점 동기화 여유 포함
		static const double MinViewDot = FMath::Cos(FMath::DegreesToRadians(75.0));
		const FVector HeadOffset(0.0, 0.0, HalfHeight * 0.85);
		const FVector SideOffset = View.Right * (Radius * 0.85);
		const FVector Samples[] = {
			Location, Location + HeadOffset, Location - HeadOffset, Location + SideOffset, Location - SideOffset
		};
		for (const FVector& Sample : Samples)
		{
			const FVector Direction = (Sample - View.Eye).GetSafeNormal();
			if (FVector::DotProduct(View.Forward, Direction) >= MinViewDot
				&& !World.LineTraceTestByChannel(View.Eye, Sample, ECC_Visibility, Query))
			{
				return true;
			}
		}
		return false;
	}
}

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
		if (!Config.bSpawnOnInterval || !Config.ActorClass || Config.PoolSize <= 0 || Config.MaxActiveCount <= 0) continue;
		if (!Config.ActorClass->ImplementsInterface(UTBPoolInterface::StaticClass()))
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
		ActivateFromPool(Index, Transform);
	}
}

AActor* UTBSpawnManagerComponent::SpawnSummonedMonster(TSubclassOf<AActor> ActorClass, const FTransform& Transform,
	float Scale, float WalkSpeedMultiplier, int32 PoolCapacity)
{
	PoolOwner = Cast<ATBLevelManager>(GetOwner());
	if (!IsValid(PoolOwner) || !PoolOwner->HasAuthority() || !ActorClass || !ActorClass->IsChildOf<ATBMonster>()
		|| PoolCapacity <= 0 || !FMath::IsFinite(Scale) || Scale <= 0.0f
		|| !FMath::IsFinite(WalkSpeedMultiplier) || WalkSpeedMultiplier <= 0.0f)
	{
		return nullptr;
	}

	int32 PoolIndex = PoolOwner->RuntimeSpawnPools.IndexOfByPredicate([ActorClass](const FTBSpawnPoolRuntime& Entry)
	{
		return !Entry.Config.bSpawnOnInterval && Entry.Config.ActorClass == ActorClass;
	});
	if (PoolIndex == INDEX_NONE)
	{
		PoolIndex = PoolOwner->RuntimeSpawnPools.AddDefaulted();
		FTBSpawnPoolConfig& Config = PoolOwner->RuntimeSpawnPools[PoolIndex].Config;
		Config.ActorClass = ActorClass;
		Config.PoolSize = PoolCapacity;
		Config.MaxActiveCount = PoolCapacity;
		Config.bSpawnOnInterval = false;
	}
	return ActivateFromPool(PoolIndex, Transform, Scale, WalkSpeedMultiplier);
}

AActor* UTBSpawnManagerComponent::ActivateFromPool(int32 PoolIndex, const FTransform& Transform,
	TOptional<float> SummonScale, float SummonWalkSpeedMultiplier)
{
	UWorld* World = GetWorld();
	if (!World || !IsValid(PoolOwner) || !PoolOwner->RuntimeSpawnPools.IsValidIndex(PoolIndex)) return nullptr;

	FTBSpawnPoolRuntime& Entry = PoolOwner->RuntimeSpawnPools[PoolIndex];
	const FTBSpawnPoolConfig Config = Entry.Config;
	Entry.ActiveActors.RemoveAll([](const TObjectPtr<AActor>& Actor) { return !IsValid(Actor); });
	Entry.AvailableActors.RemoveAll([](const TObjectPtr<AActor>& Actor) { return !IsValid(Actor); });
	if (Entry.ActiveActors.Num() >= FMath::Min(Config.MaxActiveCount, Config.PoolSize)) return nullptr;

	AActor* Actor = nullptr;
	bool bSpawnedNewActor = false;
	if (!Entry.AvailableActors.IsEmpty())
	{
		Actor = Entry.AvailableActors.Pop();
		if (!IsValid(Actor) || !Actor->SetActorLocationAndRotation(
			Transform.GetLocation(), Transform.Rotator(), false, nullptr, ETeleportType::TeleportPhysics))
		{
			if (IsValid(Actor)) PoolOwner->RuntimeSpawnPools[PoolIndex].AvailableActors.Add(Actor);
			return nullptr;
		}
	}
	else if (Entry.ActiveActors.Num() < Config.PoolSize)
	{
		FActorSpawnParameters Params;
		// The explosion caller checks the final scaled capsule before acquiring a summon.
		Params.SpawnCollisionHandlingOverride = SummonScale.IsSet()
			? ESpawnActorCollisionHandlingMethod::AlwaysSpawn : ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
		FTransform SpawnTransform = Transform;
		if (SummonScale.IsSet()) SpawnTransform.SetScale3D(FVector(SummonScale.GetValue()));
		Actor = World->SpawnActor<AActor>(Config.ActorClass, SpawnTransform, Params);
		bSpawnedNewActor = true;
	}
	if (!IsValid(Actor)) return nullptr;

	if (SummonScale.IsSet())
	{
		CastChecked<ATBMonster>(Actor)->PrepareSummonedSpawn(
			SummonScale.GetValue(), SummonWalkSpeedMultiplier, bSpawnedNewActor);
	}
	UTBPoolStateComponent* PoolState = Actor->FindComponentByClass<UTBPoolStateComponent>();
	if (!PoolState)
	{
		PoolState = NewObject<UTBPoolStateComponent>(Actor);
		Actor->AddInstanceComponent(PoolState);
		PoolState->RegisterComponent();
	}
	PoolOwner->RuntimeSpawnPools[PoolIndex].ActiveActors.Add(Actor);
	ITBPoolInterface::Execute_OnPoolActivated(Actor);
	if (!IsValid(Actor) || !IsValid(PoolState) || !PoolOwner->RuntimeSpawnPools[PoolIndex].ActiveActors.Contains(Actor)) return nullptr;
	PoolState->SetPoolActive(true);
	// Collision restoration may dispatch overlap callbacks that return the actor.
	if (!IsValid(Actor) || !PoolOwner->RuntimeSpawnPools[PoolIndex].ActiveActors.Contains(Actor)) return nullptr;
	if (ITBPoolInterface* NativePoolActor = Cast<ITBPoolInterface>(Actor)) NativePoolActor->OnPoolReady();
	if (IsValid(Actor)) Actor->ForceNetUpdate();
	return IsValid(Actor) && PoolOwner->RuntimeSpawnPools[PoolIndex].ActiveActors.Contains(Actor) ? Actor : nullptr;
}

UTBSpawnManagerComponent* UTBSpawnManagerComponent::FindForActor(AActor* Actor)
{
	if (!IsValid(Actor) || !Actor->HasAuthority() || !Actor->GetWorld()) return nullptr;
	ATBLevelManager* Manager = Cast<ATBLevelManager>(UGameplayStatics::GetActorOfClass(Actor, ATBLevelManager::StaticClass()));
	return Manager ? Manager->FindComponentByClass<UTBSpawnManagerComponent>() : nullptr;
}

bool UTBSpawnManagerComponent::ReturnActorToPool(AActor* Actor)
{
	UTBSpawnManagerComponent* SpawnManager = FindForActor(Actor);
	return IsValid(SpawnManager) && SpawnManager->ReturnToPool(Actor);
}

bool UTBSpawnManagerComponent::NotifyActorDied(AActor* Actor)
{
	UTBSpawnManagerComponent* SpawnManager = FindForActor(Actor);
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
	for (int32 Index = 0; Index < PoolOwner->RuntimeSpawnPools.Num(); ++Index)
	{
		if (PoolOwner->RuntimeSpawnPools[Index].ActiveActors.RemoveSingle(Actor) == 0) continue;
		ITBPoolInterface::Execute_OnPoolDeactivated(Actor);
		if (!IsValid(Actor)) return true;
		if (UTBPoolStateComponent* PoolState = Actor->FindComponentByClass<UTBPoolStateComponent>())
		{
			PoolState->SetPoolActive(false);
			if (IsValid(Actor) && IsValid(PoolOwner) && PoolOwner->RuntimeSpawnPools.IsValidIndex(Index))
			{
				PoolOwner->RuntimeSpawnPools[Index].AvailableActors.Add(Actor);
			}
		}
		else Actor->Destroy();
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
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!Nav || !PoolOwner->GetSpawnFloorSearchParameters(Floor, FloorBounds, MaxAttempts, TraceHeight)) return false;

	// Native character capsules retain their existing clearance. Other actors use the
	// configurable capsule and collision profile in the spawn entry.
	const AActor* Defaults = Entry.ActorClass->GetDefaultObject<AActor>();
	const APawn* PawnDefaults = Cast<APawn>(Defaults);
	const UPawnMovementComponent* Movement = PawnDefaults ? PawnDefaults->GetMovementComponent() : nullptr;
	const ANavigationData* NavData = nullptr;
	if (Movement)
	{
		FNavAgentProperties Agent = Movement->GetNavAgentPropertiesRef();
		const UCapsuleComponent* AgentCapsule = Cast<UCapsuleComponent>(PawnDefaults->GetRootComponent());
		if (AgentCapsule && Movement->ShouldUpdateNavAgentWithOwnersCollision())
		{
			Agent.AgentRadius = AgentCapsule->GetScaledCapsuleRadius();
			Agent.AgentHeight = AgentCapsule->GetScaledCapsuleHalfHeight() * 2.0f;
		}
		// GetNavDataForProps can fall back to another agent while the intended data is missing.
		const FNavDataConfig* ConfiguredAgent = Nav->GetSupportedAgents().FindByPredicate(
			[&Agent](const FNavDataConfig& Config)
			{
				return Config.bCanFly == Agent.bCanFly && Config.IsEquivalent(Agent);
			});
		NavData = ConfiguredAgent ? Nav->GetNavDataForAgentName(ConfiguredAgent->Name) : Nav->GetNavDataForProps(Agent);
		if (!ConfiguredAgent && IsValid(NavData) && (NavData->GetConfig().AgentRadius < Agent.AgentRadius
			|| NavData->GetConfig().AgentHeight < Agent.AgentHeight))
		{
			return false;
		}
	}
	else NavData = Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
	if (!IsValid(NavData)) return false;
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
	FCollisionQueryParams VisibilityQuery(SCENE_QUERY_STAT(TBPoolSpawnVisibility), false);
	TArray<FSpawnPlayerView, TInlineAllocator<4>> PlayerViews;
	const bool bCheckMonsterVisibility = Entry.ActorClass->IsChildOf<ATBMonster>();
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Controller = It->Get();
		const APawn* Player = IsValid(Controller) ? Controller->GetPawn() : nullptr;
		if (!IsValid(Player)) continue;

		FVector Eye = Player->GetPawnViewLocation();
		FRotator Rotation = Controller->GetControlRotation();
		if (bCheckMonsterVisibility && Controller->IsLocalController())
		{
			Controller->GetPlayerViewPoint(Eye, Rotation);
		}
		PlayerViews.Add({Player->GetActorLocation(), Eye, Rotation.Vector(), FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y)});
		VisibilityQuery.AddIgnoredActor(Player);
	}
	if (bCheckMonsterVisibility)
	{
		// 다른 풀 액터가 잠깐 가리는 위치에서 몬스터가 나타나는 상황 방지
		for (const FTBSpawnPoolRuntime& Pool : PoolOwner->RuntimeSpawnPools)
		{
			for (const TObjectPtr<AActor>& Actor : Pool.ActiveActors)
			{
				if (IsValid(Actor)) VisibilityQuery.AddIgnoredActor(Actor);
			}
		}
	}

	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		const double X = FMath::FRandRange(FloorBounds.Min.X, FloorBounds.Max.X);
		const double Y = FMath::FRandRange(FloorBounds.Min.Y, FloorBounds.Max.Y);
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, FVector(X, Y, FloorBounds.Max.Z + TraceHeight), FVector(X, Y, FloorBounds.Min.Z - 1000.0), ECC_Visibility, Query) || Hit.GetActor() != Floor) continue;
		FNavLocation NavLocation;
		if (!Nav->ProjectPointToNavigation(Hit.ImpactPoint, NavLocation, NavData->GetConfig().DefaultQueryExtent, NavData)) continue;
		if (FVector::DistSquared2D(Hit.ImpactPoint, NavLocation.Location) > FMath::Square(25.0f) || FMath::Abs(Hit.ImpactPoint.Z - NavLocation.Location.Z) > 50.0f) continue;
		const FVector Location = NavLocation.Location + FVector(0.0, 0.0, HalfHeight + 3.0f);
		bool bTooClose = false;
		for (const FSpawnPlayerView& View : PlayerViews)
		{
			if (FVector::DistSquared2D(Location, View.PawnLocation) < FMath::Square(1500.0f)) { bTooClose = true; break; }
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
		if (bTooClose) continue;
		const bool bBlocked = Capsule
			? World->OverlapAnyTestByChannel(Location, FQuat::Identity, Capsule->GetCollisionObjectType(), Shape,
				Query, FCollisionResponseParams(Capsule->GetCollisionResponseToChannels()))
			: World->OverlapAnyTestByProfile(Location, FQuat::Identity, CollisionProfile, Shape, Query);
		if (bBlocked) continue;
		if (bCheckMonsterVisibility && PlayerViews.ContainsByPredicate([&](const FSpawnPlayerView& View)
		{
			return IsSpawnVisible(*World, View, Location, Radius, HalfHeight, VisibilityQuery);
		})) continue;
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
