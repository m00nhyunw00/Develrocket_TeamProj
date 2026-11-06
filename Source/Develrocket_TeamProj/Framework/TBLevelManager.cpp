#include "Framework/TBLevelManager.h"

#include "Kismet/GameplayStatics.h"
#include "Components/BrushComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Volume.h"
#include "Net/UnrealNetwork.h"
#include "PCG/CityLayoutCoordinator.h"

ATBLevelManager::ATBLevelManager()
{
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
}

void ATBLevelManager::HandleCityReady(int32 Seed, int32 Revision)
{
	if (bLocalCityReady)
	{
		return;   // 중복 방지
	}

	bLocalCityReady = true;
	UE_LOG(LogTemp, Log, TEXT("[TBLevel] Local city ready (Seed=%d, Rev=%d)"), Seed, Revision);

	OnLocalCityReadyEvent.Broadcast();   // 위젯: "START 활성화해도 됨"
}

bool ATBLevelManager::StartMatch()
{
	// 서버만, 한 번만, 도시가 준비된 뒤에만
	if (!HasAuthority() || bMatchStarted || !bLocalCityReady)
	{
		return false;
	}
	if (!PlacePlayersAroundCity())
	{
		UE_LOG(LogTemp, Warning, TEXT("[TBLevel] 외곽 스폰 위치를 찾지 못해 게임을 시작하지 않았습니다."));
		return false;
	}

	bMatchStarted = true;

	// 서버(호스트)는 OnRep이 자동 호출되지 않으므로 직접 호출
	OnRep_MatchStarted();
	return true;
}

bool ATBLevelManager::PlacePlayersAroundCity()
{
	UWorld* World = GetWorld();
	ACityLayoutCoordinator* City = World ? Cast<ACityLayoutCoordinator>(UGameplayStatics::GetActorOfClass(this, ACityLayoutCoordinator::StaticClass())) : nullptr;
	AVolume* Volume = City ? Cast<AVolume>(City->CityVolume) : nullptr;
	UBrushComponent* Brush = Volume ? Volume->GetBrushComponent() : nullptr;
	if (!World || !Brush)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TBLevel] CityVolume 또는 볼륨 경계를 찾지 못했습니다."));
		return false;
	}
	const FBox Bounds = Brush->Bounds.GetBox();
	if (!Bounds.IsValid || Bounds.GetExtent().X <= 1.0f || Bounds.GetExtent().Y <= 1.0f)
	{
		return false;
	}

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

	const int32 EdgeOrder[] = { 0, 2, 1, 3 };
	bool UsedEdges[] = { false, false, false, false };
	TArray<FVector> Locations;
	for (int32 Index = 0; Index < Characters.Num(); ++Index)
	{
		FVector Location;
		bool bFound = false;
		for (int32 Offset = 0; Offset < 4 && !bFound; ++Offset)
		{
			const int32 Edge = EdgeOrder[(Index + Offset) % 4];
			if (Index < 4 && UsedEdges[Edge]) continue;
			bFound = FindSafeEdgeLocation(Bounds, Edge, Characters[Index], Locations, IgnoredPlayers, Location);
			if (bFound) UsedEdges[Edge] = true;
		}
		if (!bFound) return false;
		Locations.Add(Location);
	}

	TArray<FTransform> OriginalTransforms;
	TArray<FRotator> OriginalControlRotations;
	for (ACharacter* Character : Characters) OriginalTransforms.Add(Character->GetActorTransform());
	for (ACharacter* Character : Characters) OriginalControlRotations.Add(Character->GetControlRotation());
	for (int32 Index = 0; Index < Characters.Num(); ++Index)
	{
		const FVector Direction = Bounds.GetCenter() - Locations[Index];
		const FRotator Facing(0.0f, Direction.Rotation().Yaw, 0.0f);
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

bool ATBLevelManager::FindSafeEdgeLocation(const FBox& CityBounds, int32 Edge, const ACharacter* Character, const TArray<FVector>& ReservedLocations, const TArray<AActor*>& Players, FVector& OutLocation) const
{
	const FVector Center = CityBounds.GetCenter();
	const FVector Extent = CityBounds.GetExtent();
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(CityPlayerSpawn), false);
	Query.AddIgnoredActors(Players);
	constexpr int32 Samples = 17;
	for (int32 Index = 0; Index < Samples; ++Index)
	{
		const float Along = ((Index + 0.5f) / Samples * 1.6f) - 0.8f;
		const float XInset = FMath::Min(SpawnEdgeInset, static_cast<float>(Extent.X * 0.45));
		const float YInset = FMath::Min(SpawnEdgeInset, static_cast<float>(Extent.Y * 0.45));
		FVector Sample = Center;
		if (Edge == 0 || Edge == 2) { Sample.X += (Edge == 0 ? 1.0 : -1.0) * (Extent.X - XInset); Sample.Y += Along * Extent.Y; }
		else { Sample.Y += (Edge == 1 ? 1.0 : -1.0) * (Extent.Y - YInset); Sample.X += Along * Extent.X; }
		FHitResult Hit;
		const FVector Top(Sample.X, Sample.Y, CityBounds.Max.Z + 500.0);
		const FVector Bottom(Sample.X, Sample.Y, CityBounds.Min.Z - 1000.0);
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Top, Bottom, ECC_Visibility, Query) || Hit.ImpactNormal.Z < 0.7f) continue;
		const FVector Location = Hit.ImpactPoint + FVector(0, 0, HalfHeight + 3.0f);
		bool bTooClose = false;
		for (const FVector& Reserved : ReservedLocations) bTooClose |= FVector::Dist2D(Location, Reserved) < MinimumPlayerSpawnDistance;
		if (bTooClose) continue;
		const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
		if (GetWorld()->OverlapBlockingTestByProfile(Location, FQuat::Identity, Capsule->GetCollisionProfileName(), Shape, Query)) continue;
		OutLocation = Location;
		return true;
	}
	return false;
}

void ATBLevelManager::OnRep_MatchStarted()
{
	UE_LOG(LogTemp, Log, TEXT("[TBLevel] Match started"));
	OnMatchStartedEvent.Broadcast();
}
