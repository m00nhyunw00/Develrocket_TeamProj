#include "Framework/TBLevelManager.h"

#include "Kismet/GameplayStatics.h"
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

void ATBLevelManager::StartMatch()
{
	// 서버만, 한 번만, 도시가 준비된 뒤에만
	if (!HasAuthority() || bMatchStarted || !bLocalCityReady)
	{
		return;
	}

	bMatchStarted = true;

	// 서버(호스트)는 OnRep이 자동 호출되지 않으므로 직접 호출
	OnRep_MatchStarted();
}

void ATBLevelManager::OnRep_MatchStarted()
{
	UE_LOG(LogTemp, Log, TEXT("[TBLevel] Match started"));
	OnMatchStartedEvent.Broadcast();
}