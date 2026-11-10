#include "Framework/TBLevelManager.h"
#include "Components/TBSpawnManagerComponent.h"
#include "Framework/TBSpawnPoolDataAsset.h"

#include "Kismet/GameplayStatics.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Character/Monster/TBMonster.h"
#include "Character/Player/TBPlayer.h"
#include "Net/UnrealNetwork.h"
#include "PCG/CityLayoutCoordinator.h"
#include "TimerManager.h"

ATBLevelManager::ATBLevelManager()
{
	SpawnManager = CreateDefaultSubobject<UTBSpawnManagerComponent>(TEXT("SpawnManager"));
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = false;
}

void ATBLevelManager::BeginPlay()
{
	Super::BeginPlay();
	
	// 맵에 있는 Coordinator 하나 찾기
	ACityLayoutCoordinator* City = Cast<ACityLayoutCoordinator>(
		UGameplayStatics::GetActorOfClass(this, ACityLayoutCoordinator::StaticClass()));
	
	if (!City)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TBLevel] CityLayoutCoordinator not found"));
		return;
	}
	
	// 이미 생성이 끝났으면 바로 처리
	if (City->bLocalCityReady==true)
	{
		HandleCityReady(City->Layout.Seed, City->Layout.Revision);
	}
	else
	{
		// Dynamic 델리게이트 구축
		City->OnLocalCityReady.AddDynamic(this, &ATBLevelManager::HandleCityReady);
	}
}

void ATBLevelManager::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATBLevelManager, bMatchStarted);
	DOREPLIFETIME(ATBLevelManager, bSearchingForSpawn);
}

void ATBLevelManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(MatchSpawnRetryTimer);
	GetWorldTimerManager().ClearTimer(PendingSpawnRetryTimer);
	PendingSpawnRequests.Empty();
	Super::EndPlay(EndPlayReason);
}

void ATBLevelManager::HandleCityReady(int32 Seed, int32 Revision)
{
	if (bLocalCityReady)
	{
		return;   // 중복 방지
	}

	bLocalCityReady = true;
	if (HasAuthority() && SpawnManager)
	{
		// 도시 생성 완료 후 서버에서 풀을 준비하고 스폰 검사를 시작합니다.
		InitializeSpawnPools();
		SpawnManager->StartSpawning();
	}
	UE_LOG(LogTemp, Log, TEXT("[TBLevel] Local city ready (Seed=%d, Rev=%d)"), Seed, Revision);

	OnLocalCityReadyEvent.Broadcast();   // 위젯: "START 활성화해도 됨"
}

bool ATBLevelManager::HasConfiguredSpawnPools() const
{
	// 데이터 에셋이 있으면 빈 배열이어도 이전 설정을 대신 사용하지 않습니다.
	return SpawnPoolDataAsset ? !SpawnPoolDataAsset->SpawnEntries.IsEmpty() : !SpawnPools.IsEmpty();
}

void ATBLevelManager::InitializeSpawnPools()
{
	// 이미 생성된 런타임 풀의 액터 목록을 잃지 않도록 한 번만 초기화합니다.
	if (!HasAuthority() || bSpawnPoolsInitialized) return;
	bSpawnPoolsInitialized = true;
	if (SpawnPoolDataAsset)
	{
		if (!SpawnPools.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("[TBLevel] Spawn pool data asset takes precedence over legacy instance settings."));
		}
		for (const FTBSpawnPoolConfig& Config : SpawnPoolDataAsset->SpawnEntries)
		{
			RuntimeSpawnPools.AddDefaulted_GetRef().Config = Config;
		}
		return;
	}
	// 데이터 에셋이 없을 때만 이전 레벨 인스턴스의 설정을 복사합니다.
	for (const FTBSpawnPoolEntry& Legacy : SpawnPools)
	{
		FTBSpawnPoolConfig& Config = RuntimeSpawnPools.AddDefaulted_GetRef().Config;
		Config.ActorClass = Legacy.ActorClass;
		Config.MaxActiveCount = Legacy.MaxActiveCount;
		Config.PoolSize = Legacy.PoolSize;
		Config.SpawnInterval = Legacy.SpawnInterval;
		Config.SpawnPointTag = Legacy.SpawnPointTag;
	}
}
bool ATBLevelManager::StartMatch()
{
	// 서버만, 한 번만, 도시가 준비된 뒤에만
	if (!HasAuthority() || bMatchStarted || !bLocalCityReady)
	{
		return false;
	}
	if (bSearchingForSpawn) return true;
	bSearchingForSpawn = true;
	MatchRetryCount = 0;
	OnRep_SearchingForSpawn();
	ForceNetUpdate();
	RetryMatchSpawn();
	if (bSearchingForSpawn)
	{
		GetWorldTimerManager().SetTimer(MatchSpawnRetryTimer, this, &ThisClass::RetryMatchSpawn, FMath::Max(0.1f, SpawnRetryInterval), true);
	}
	return true;
}

void ATBLevelManager::RetryMatchSpawn()
{
	if (!HasAuthority() || !bSearchingForSpawn || bMatchStarted) return;
	if (!PlacePlayersAroundCity())
	{
		if (++MatchRetryCount % 10 == 0) UE_LOG(LogTemp, Warning, TEXT("[TBLevel] 안전한 시작 위치 탐색을 계속 재시도 중입니다. Floor 설정과 주변 공간을 확인하세요. (%d회)"), MatchRetryCount);
		return;
	}
	GetWorldTimerManager().ClearTimer(MatchSpawnRetryTimer);
	bSearchingForSpawn = false;
	bMatchStarted = true;
	OnRep_SearchingForSpawn();
	OnRep_MatchStarted();
	ForceNetUpdate();
}

void ATBLevelManager::OnRep_SearchingForSpawn()
{
	OnSpawnSearchChangedEvent.Broadcast();
}

bool ATBLevelManager::PlacePlayersAroundCity()
{
	UWorld* World = GetWorld();
	if (!World) return false;

	TArray<ACharacter*> Characters;
	TArray<AActor*> IgnoredPlayers;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!IsValid(PC)) continue;
		ACharacter* Character = Cast<ACharacter>(PC->GetPawn());
		if (!IsValid(Character) || !Character->GetCapsuleComponent())
		{
			UE_LOG(LogTemp, Warning, TEXT("[TBLevel] 플레이어 Pawn이 준비되지 않았습니다."));
			return false;
		}
		Characters.Add(Character);
		IgnoredPlayers.Add(Character);
	}
	if (Characters.IsEmpty()) return false;

	TArray<FVector> Locations;
	TArray<FTransform> SpawnTransforms;
	for (int32 Index = 0; Index < Characters.Num(); ++Index)
	{
		FTransform SpawnTransform;
		if (!FindSafePlayerSpawnInternal(Characters[Index], Characters[Index], IgnoredPlayers, Locations, SpawnTransform)) return false;
		SpawnTransforms.Add(SpawnTransform);
		Locations.Add(SpawnTransform.GetLocation());
	}

	TArray<FTransform> OriginalTransforms;
	TArray<FRotator> OriginalControlRotations;
	for (ACharacter* Character : Characters) OriginalTransforms.Add(Character->GetActorTransform());
	for (ACharacter* Character : Characters) OriginalControlRotations.Add(Character->GetControlRotation());
	for (int32 Index = 0; Index < Characters.Num(); ++Index)
	{
		const FRotator Facing = SpawnTransforms[Index].Rotator();
		if (!Characters[Index]->TeleportTo(Locations[Index], Facing, false, false))
		{
			for (int32 Previous = 0; Previous < Index; ++Previous)
			{
				Characters[Previous]->SetActorTransform(OriginalTransforms[Previous], false, nullptr, ETeleportType::TeleportPhysics);
				if (AController* Controller = Characters[Previous]->GetController()) Controller->SetControlRotation(OriginalControlRotations[Previous]);
			}
			return false;
		}
		if (AController* Controller = Characters[Index]->GetController()) Controller->SetControlRotation(Facing);
	}
	return true;
}

bool ATBLevelManager::FindSafePlayerSpawn(TSubclassOf<ACharacter> CharacterClass, AActor* IgnoredActor, FTransform& OutSpawnTransform) const
{
	if (!HasAuthority() || !bLocalCityReady || !CharacterClass) return false;
	const ACharacter* Character = CharacterClass->GetDefaultObject<ACharacter>();
	return FindSafePlayerSpawnInternal(Character, IgnoredActor, {}, {}, OutSpawnTransform);
}

bool ATBLevelManager::RequestSafePlayerSpawn(TSubclassOf<ACharacter> CharacterClass, AActor* IgnoredActor, FTBSpawnLocationReady OnReady)
{
	if (!HasAuthority() || !CharacterClass || !OnReady.IsBound()) return false;
	FTBPendingSpawnRequest& Request = PendingSpawnRequests.AddDefaulted_GetRef();
	Request.CharacterClass = CharacterClass;
	Request.IgnoredActor = IgnoredActor;
	Request.OnReady = OnReady;
	if (!GetWorldTimerManager().IsTimerActive(PendingSpawnRetryTimer))
	{
		GetWorldTimerManager().SetTimer(PendingSpawnRetryTimer, this, &ThisClass::RetryPendingSpawns, FMath::Max(0.1f, SpawnRetryInterval), true, 0.01f);
	}
	return true;
}

void ATBLevelManager::RetryPendingSpawns()
{
	if (!HasAuthority()) return;
	TArray<FTBPendingSpawnRequest> Requests = MoveTemp(PendingSpawnRequests);
	PendingSpawnRequests.Reset();
	TArray<FVector> ReservedLocations;
	for (const FTBPendingSpawnRequest& Request : Requests)
	{
		if (!Request.OnReady.IsBound() || !Request.CharacterClass) continue;
		FTransform SpawnTransform;
		const ACharacter* Character = Request.CharacterClass->GetDefaultObject<ACharacter>();
		if (bLocalCityReady && FindSafePlayerSpawnInternal(Character, Request.IgnoredActor.Get(), {}, ReservedLocations, SpawnTransform))
		{
			ReservedLocations.Add(SpawnTransform.GetLocation());
			Request.OnReady.ExecuteIfBound(SpawnTransform);
		}
		else PendingSpawnRequests.Add(Request);
	}
	if (PendingSpawnRequests.IsEmpty())
	{
		GetWorldTimerManager().ClearTimer(PendingSpawnRetryTimer);
		PendingRetryCount = 0;
	}
	else if (++PendingRetryCount % 10 == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TBLevel] 리스폰 위치를 계속 탐색 중입니다. Floor 설정과 주변 공간을 확인하세요. (대기 %d명)"), PendingSpawnRequests.Num());
	}
}

bool ATBLevelManager::GetSpawnFloorSearchParameters(AActor*& OutFloor, FBox& OutBounds, int32& OutMaxAttempts, float& OutTraceHeight) const
{
	OutFloor = SpawnFloor.Get();
	if (!IsValid(OutFloor))
	{
		TArray<AActor*> Floors;
		if (!SpawnFloorTag.IsNone()) UGameplayStatics::GetAllActorsWithTag(this, SpawnFloorTag, Floors);
		if (Floors.Num() != 1) return false;
		OutFloor = Floors[0];
	}
	if (!IsValid(OutFloor)) return false;
	OutBounds = OutFloor->GetComponentsBoundingBox(false, false);
	if (!OutBounds.IsValid || OutBounds.GetExtent().X <= 1.0 || OutBounds.GetExtent().Y <= 1.0) return false;
	OutMaxAttempts = MaxSpawnAttempts;
	OutTraceHeight = TraceHeightAboveCity;
	return true;
}
bool ATBLevelManager::FindSafePlayerSpawnInternal(const ACharacter* Character, AActor* IgnoredActor, const TArray<AActor*>& IgnoredPlayers, const TArray<FVector>& ReservedLocations, FTransform& OutSpawnTransform) const
{
	UWorld* World = GetWorld();
	if (!World || !Character || !Character->GetCapsuleComponent()) return false;

	AActor* Floor = nullptr;
	FBox FloorBounds(ForceInit);
	int32 SearchAttempts = 0;
	float TraceHeight = 0.0f;
	if (!GetSpawnFloorSearchParameters(Floor, FloorBounds, SearchAttempts, TraceHeight)) return false;

	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(CityPlayerSpawn), false);
	Query.AddIgnoredActors(IgnoredPlayers);
	if (IsValid(IgnoredActor)) Query.AddIgnoredActor(IgnoredActor);
	for (int32 Attempt = 0; Attempt < SearchAttempts; ++Attempt)
	{
		const double X = FMath::FRandRange(FloorBounds.Min.X, FloorBounds.Max.X);
		const double Y = FMath::FRandRange(FloorBounds.Min.Y, FloorBounds.Max.Y);
		FHitResult Hit;
		const FVector Top(X, Y, FloorBounds.Max.Z + TraceHeight);
		const FVector Bottom(X, Y, FloorBounds.Min.Z - 1000.0);
		if (!World->LineTraceSingleByChannel(Hit, Top, Bottom, ECC_Visibility, Query) || Hit.GetActor() != Floor) continue;
		const FVector Location = Hit.ImpactPoint + FVector(0, 0, HalfHeight + 3.0f);
		bool bTooClose = false;
		for (const FVector& Reserved : ReservedLocations) bTooClose |= FVector::Dist2D(Location, Reserved) < MinimumPlayerSpawnDistance;
		for (TActorIterator<ATBPlayer> It(World); It && !bTooClose; ++It)
		{
			if (*It != IgnoredActor && !IgnoredPlayers.Contains(*It) && FVector::Dist2D(Location, It->GetActorLocation()) < MinimumPlayerSpawnDistance) bTooClose = true;
		}
		for (TActorIterator<ATBMonster> It(World); It && !bTooClose; ++It)
		{
			if (*It != IgnoredActor && FVector::Dist2D(Location, It->GetActorLocation()) < MinimumMonsterSpawnDistance) bTooClose = true;
		}
		if (bTooClose) continue;
		const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
		if (World->OverlapAnyTestByProfile(Location, FQuat::Identity, Capsule->GetCollisionProfileName(), Shape, Query)) continue;
		const FVector Direction = FloorBounds.GetCenter() - Location;
		OutSpawnTransform = FTransform(FRotator(0.0f, Direction.Rotation().Yaw, 0.0f), Location);
		return true;
	}
	return false;
}

void ATBLevelManager::OnRep_MatchStarted()
{
	UE_LOG(LogTemp, Log, TEXT("[TBLevel] Match started"));
	OnMatchStartedEvent.Broadcast();
}
