#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/TBClassChangeTypes.h"
#include "TBLobbyManager.generated.h"

class AGameModeBase;
class AController;
class APlayerController;
class APlayerState;
class UTBCharacterClassData;

// 로비 준비 목록 한 줄
USTRUCT()
struct FTBLobbyReadyEntry
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<APlayerState> PlayerState;

	UPROPERTY()
	bool bReady = false;
	
	// 선택한 직업
	UPROPERTY()
	TObjectPtr<const UTBCharacterClassData> ClassData;
	
	// 서버 전용 연타 방지
	double LastClassChangeTime = -1000.0;
};

// 위젯이 구독할 로컬 알림 
DECLARE_MULTICAST_DELEGATE(FOnTBLobbyChanged);

UCLASS()
class DEVELROCKET_TEAMPROJ_API ATBLobbyManager : public AActor
{
	GENERATED_BODY()

public:
	ATBLobbyManager();
	void ToggleReady(APlayerState* PlayerState);
	bool CanStartGame() const;
	void StartGame();

	const TArray<FTBLobbyReadyEntry>& GetEntries() const { return Entries; }
	int32 GetReadyCount() const;
	int32 GetMinPlayersToStart() const { return MinPlayersToStart; }

	FOnTBLobbyChanged OnLobbyChangedEvent;
	
	// [서버] 직업 변경 결재: 규칙 확인 + 장부 기록(직업, 준비 해제)
	// 실제 교체(GAS 정리, 폰 재스폰)는 요청자가 승인 후 진행
	ETBClassChangeResult ApproveClassChange(APlayerState* PlayerState, const UTBCharacterClassData* NewClassData);

	const TArray<TObjectPtr<UTBCharacterClassData>>& GetAvailableClasses() const { return AvailableClasses; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 게임 시작 최소 인원
	UPROPERTY(EditInstanceOnly, Category = "Lobby", meta = (ClampMin = "1"))
	int32 MinPlayersToStart = 4;

	// 로비에서 Battlefield 이동
	UPROPERTY(EditInstanceOnly, Category = "Lobby")
	FString BattlefieldMapPath = TEXT("/Game/_Game/Maps/L_Battlefield");

	// 로비에서 고를 수 있는 직업
	UPROPERTY(EditInstanceOnly, Category = "Lobby|Class")
	TArray<TObjectPtr<UTBCharacterClassData>> AvailableClasses;

	UPROPERTY(EditInstanceOnly, Category = "Lobby|Class", meta = (ClampMin = "0.0", Units = "s"))
	float ClassChangeCooldown = 0.5f;

private:
	void HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);
	void HandleLogout(AGameModeBase* GameMode, AController* Exiting);
	
	void AddEntry(APlayerState* PlayerState);
	void RemoveEntry(APlayerState* PlayerState);
	FTBLobbyReadyEntry* FindEntry(const APlayerState* PlayerState);
	
	void NotifyChanged();

	UFUNCTION()
	void OnRep_Entries();

	UPROPERTY(ReplicatedUsing = OnRep_Entries)
	TArray<FTBLobbyReadyEntry> Entries;

	// 맵 이동 중복 방지
	bool bTravelStarted = false;

	FDelegateHandle PostLoginHandle;
	FDelegateHandle LogoutHandle;
};