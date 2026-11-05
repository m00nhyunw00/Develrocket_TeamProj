#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TBLevelManager.generated.h"

class ACityLayoutCoordinator;

// 위젯이 구독할 로컬 알림 (일반 멀티캐스트)
DECLARE_MULTICAST_DELEGATE(FOnTBLocalCityReady);
DECLARE_MULTICAST_DELEGATE(FOnTBMatchStarted);

UCLASS()
class DEVELROCKET_TEAMPROJ_API ATBLevelManager : public AActor
{
	GENERATED_BODY()
	
public:
	ATBLevelManager();
	
	bool IsLocalCityReady() const { return bLocalCityReady; }
	bool IsMatchStarted() const { return bMatchStarted; }
	
	// 호스트(서버)의 START버튼이 호출
	void StartMatch();
	
	FOnTBLocalCityReady OnLocalCityReadyEvent;
	FOnTBMatchStarted OnMatchStartedEvent;
	
protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
private:
	// Coordinator의 OnLocalCityReady는 Dynamic(Seed, Revision) -> 시그니처를 맞춰야 함
	UFUNCTION()
	void HandleCityReady(int32 Seed, int32 Revision);
	
	UFUNCTION()
	void OnRep_MatchStarted();
	
	UPROPERTY(ReplicatedUsing=OnRep_MatchStarted)
	bool bMatchStarted = false;
	
	bool bLocalCityReady = false;
};