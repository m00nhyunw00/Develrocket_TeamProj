#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TBLobbyManager.generated.h"

class AGameModeBase;
class AController;
class APlayerController;
class APlayerState;

// 로비 준비 목록 한 줄
USTRUCT()
struct FTBLobbyReadyEntry
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<APlayerState> PlayerState;

	UPROPERTY()
	bool bReady = false;
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