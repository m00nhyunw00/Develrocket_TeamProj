#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Framework/TBSpawnPoolData.h"
#include "TBLevelManager.generated.h"

class ACityLayoutCoordinator;
class ACharacter;
class UTBSpawnManagerComponent;
class UTBSpawnPoolDataAsset;
class AGameModeBase;
class APlayerController;

// 위젯이 구독할 로컬 알림 (일반 멀티캐스트)
DECLARE_MULTICAST_DELEGATE(FOnTBLocalCityReady);
DECLARE_MULTICAST_DELEGATE(FOnTBMatchStarted);
DECLARE_MULTICAST_DELEGATE(FOnTBSpawnSearchChanged);
DECLARE_DYNAMIC_DELEGATE_OneParam(FTBSpawnLocationReady, FTransform, SpawnTransform);
DECLARE_MULTICAST_DELEGATE(FOnTBAutoStartModeChanged);

USTRUCT()
struct FTBPendingSpawnRequest
{
	GENERATED_BODY()
	UPROPERTY() TSubclassOf<ACharacter> CharacterClass;
	UPROPERTY() TWeakObjectPtr<AActor> IgnoredActor;
	UPROPERTY() FTBSpawnLocationReady OnReady;
};

UCLASS()
class DEVELROCKET_TEAMPROJ_API ATBLevelManager : public AActor
{
	GENERATED_BODY()
	
public:
	ATBLevelManager();

	// 이전 레벨 인스턴스의 설정을 보존합니다. 데이터 에셋이 지정되지 않을 때 사용합니다.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Spawn|Pool", meta=(DisplayName="Legacy Spawn Pools"))
	TArray<FTBSpawnPoolEntry> SpawnPools;

	// 지정된 데이터 에셋은 기존 레벨 인스턴스 설정보다 우선합니다.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Spawn|Pool")
	TObjectPtr<UTBSpawnPoolDataAsset> SpawnPoolDataAsset;

	// 서버의 현재 매치에서 사용하는 풀 설정 복사본과 액터 목록입니다.
	UPROPERTY(Transient)
	TArray<FTBSpawnPoolRuntime> RuntimeSpawnPools;

	// 데이터 에셋이 있으면 그 항목을, 없으면 이전 인스턴스 설정을 확인합니다.
	bool HasConfiguredSpawnPools() const;

	// 이 레벨 매니저의 런타임 풀을 참조해 스폰과 반환을 관리합니다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Spawn|Pool")
	TObjectPtr<UTBSpawnManagerComponent> SpawnManager;
	
	// 플레이어와 몬스터가 공유할 바닥 액터, 범위 및 탐색 설정을 반환합니다.
	bool GetSpawnFloorSearchParameters(AActor*& OutFloor, FBox& OutBounds, int32& OutMaxAttempts, float& OutTraceHeight) const;

	bool IsLocalCityReady() const { return bLocalCityReady; }
	bool IsMatchStarted() const { return bMatchStarted; }
	bool IsSearchingForSpawn() const { return bSearchingForSpawn; }
	
	// 호스트(서버)의 START버튼이 호출
	bool StartMatch();

	// 서버에서 호출. 리스폰할 캐릭터 클래스의 캡슐을 기준으로 위치 탐색.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="City|Player Spawn")
	bool FindSafePlayerSpawn(TSubclassOf<ACharacter> CharacterClass, AActor* IgnoredActor, FTransform& OutSpawnTransform) const;

	// 리스폰 위치를 찾을 때까지 재시도하고 성공 시 콜백 호출
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="City|Player Spawn")
	bool RequestSafePlayerSpawn(TSubclassOf<ACharacter> CharacterClass, AActor* IgnoredActor, FTBSpawnLocationReady OnReady);
	
	// 로비를 거쳐 들어와 자동 시작하는 매치인지 (위젯이 START 버튼 대신 대기 문구 표시)
	bool IsAutoStartMode() const { return bAutoStartMode; }

	FOnTBAutoStartModeChanged OnAutoStartModeChangedEvent;
	
	FOnTBLocalCityReady OnLocalCityReadyEvent;
	FOnTBMatchStarted OnMatchStartedEvent;
	FOnTBSpawnSearchChanged OnSpawnSearchChangedEvent;
	
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditInstanceOnly, Category="City|Player Spawn", meta=(ClampMin="0.0", Units="cm"))
	float MinimumPlayerSpawnDistance = 3000.0f;

	UPROPERTY(EditInstanceOnly, Category="City|Player Spawn", meta=(ClampMin="0.0", Units="cm"))
	float MinimumMonsterSpawnDistance = 3000.0f;

	UPROPERTY(EditInstanceOnly, Category="City|Player Spawn", meta=(ClampMin="1"))
	int32 MaxSpawnAttempts = 100;

	UPROPERTY(EditInstanceOnly, Category="City|Player Spawn", meta=(ClampMin="0.1", Units="s"))
	float SpawnRetryInterval = 1.0f;

	UPROPERTY(EditInstanceOnly, Category="City|Player Spawn")
	TObjectPtr<AActor> SpawnFloor;

	UPROPERTY(EditInstanceOnly, Category="City|Player Spawn")
	FName SpawnFloorTag = TEXT("CityFloor");

	UPROPERTY(EditInstanceOnly, Category="City|Player Spawn", meta=(ClampMin="0.0", Units="cm"))
	float TraceHeightAboveCity = 10000.0f;
	
	UPROPERTY(EditInstanceOnly, Category="Match|AutoStart", meta=(ClampMin="0.0", Units="s"))
	float AutoStartTimeout = 30.0f;
	
private:
	// Coordinator의 OnLocalCityReady는 Dynamic(Seed, Revision) -> 시그니처를 맞춰야 함
	UFUNCTION()
	void HandleCityReady(int32 Seed, int32 Revision);
	
	UFUNCTION()
	void OnRep_MatchStarted();

	UFUNCTION()
	void OnRep_SearchingForSpawn();
	// 도시 생성 완료 시 서버에서 설정을 런타임 풀에 한 번 복사합니다.
	void InitializeSpawnPools();
	bool bSpawnPoolsInitialized = false;
	void RetryMatchSpawn();
	void RetryPendingSpawns();

	bool PlacePlayersAroundCity();
	bool FindSafePlayerSpawnInternal(const ACharacter* Character, AActor* IgnoredActor, const TArray<AActor*>& IgnoredPlayers, const TArray<FVector>& ReservedLocations, FTransform& OutSpawnTransform) const;
	
	UPROPERTY(ReplicatedUsing=OnRep_MatchStarted)
	bool bMatchStarted = false;

	UPROPERTY(ReplicatedUsing=OnRep_SearchingForSpawn)
	bool bSearchingForSpawn = false;

	UPROPERTY(Transient)
	TArray<FTBPendingSpawnRequest> PendingSpawnRequests;
	FTimerHandle MatchSpawnRetryTimer;
	FTimerHandle PendingSpawnRetryTimer;
	int32 MatchRetryCount = 0;
	int32 PendingRetryCount = 0;
	
	bool bLocalCityReady = false;
	
	// 자동 시작 ----------------------

	// [서버] ServerTravel URL의 ExpectedPlayers 옵션을 읽어 자동 시작 모드 설정
	void InitializeAutoStart();
	// [서버] 조건이 맞으면 StartMatch (도시 준비 / 접속 / 타임아웃 때 호출)
	void TryAutoStart();
	void HandleAutoStartTimeout();
	void HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);

	UFUNCTION()
	void OnRep_AutoStartMode();

	UPROPERTY(ReplicatedUsing=OnRep_AutoStartMode)
	bool bAutoStartMode = false;

	int32 ExpectedPlayers = 0;      // 서버 전용
	bool bForceAutoStart = false;   // 타임아웃 후 인원 조건 무시
	FTimerHandle AutoStartTimeoutHandle;
	FDelegateHandle PostLoginHandle;
};
