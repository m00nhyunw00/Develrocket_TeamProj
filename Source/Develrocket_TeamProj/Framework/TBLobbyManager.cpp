#include "Framework/TBLobbyManager.h"

#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

ATBLobbyManager::ATBLobbyManager()
{
	bReplicates = true;
	// 거리와 상관없이 모든 클라이언트에 복제
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = false;
}

void ATBLobbyManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATBLobbyManager, Entries);
}

void ATBLobbyManager::BeginPlay()
{
	Super::BeginPlay();

	// 목록 관리는 서버만
	if (!HasAuthority())
	{
		return;
	}

	// 호스트는 이 BeginPlay보다 먼저 로그인함 (LoadMap: SpawnPlayActor → BeginPlay 순서)
	// → 이벤트를 놓친 플레이어를 PlayerArray로 먼저 채움
	if (const AGameStateBase* GameState = GetWorld()->GetGameState())
	{
		for (APlayerState* PlayerState : GameState->PlayerArray)
		{
			AddEntry(PlayerState);
		}
	}

	// 이후 입장/퇴장은 엔진 전역 이벤트로 받음
	PostLoginHandle = FGameModeEvents::GameModePostLoginEvent.AddUObject(this, &ThisClass::HandlePostLogin);
	LogoutHandle = FGameModeEvents::GameModeLogoutEvent.AddUObject(this, &ThisClass::HandleLogout);

	NotifyChanged();
}

void ATBLobbyManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 전역(static) 이벤트라서 해제하지 않으면 파괴된 뒤에도 호출될 수 있음
	FGameModeEvents::GameModePostLoginEvent.Remove(PostLoginHandle);
	FGameModeEvents::GameModeLogoutEvent.Remove(LogoutHandle);

	Super::EndPlay(EndPlayReason);
}

// 입장 / 퇴장 ----------------------

void ATBLobbyManager::HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer)
{
	// 같은 프로세스의 다른 월드(PIE 단일 프로세스) 이벤트는 무시
	if (!GameMode || GameMode->GetWorld() != GetWorld() || !NewPlayer)
	{
		return;
	}

	// 이 시점에 PlayerState는 있지만 Pawn은 아직 없음 (로비는 PlayerState만 사용)
	AddEntry(NewPlayer->GetPlayerState<APlayerState>());
	NotifyChanged();
}

void ATBLobbyManager::HandleLogout(AGameModeBase* GameMode, AController* Exiting)
{
	if (!GameMode || GameMode->GetWorld() != GetWorld() || !Exiting)
	{
		return;
	}

	RemoveEntry(Exiting->GetPlayerState<APlayerState>());
	NotifyChanged();
}

// 목록 조작 ----------------------

void ATBLobbyManager::AddEntry(APlayerState* PlayerState)
{
	// BeginPlay 초기화와 PostLogin이 겹쳐도 한 번만 추가
	if (!IsValid(PlayerState) || FindEntry(PlayerState))
	{
		return;
	}

	FTBLobbyReadyEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.PlayerState = PlayerState;
	NewEntry.bReady = false;
}

void ATBLobbyManager::RemoveEntry(APlayerState* PlayerState)
{
	if (!PlayerState)
	{
		return;
	}

	Entries.RemoveAll([PlayerState](const FTBLobbyReadyEntry& Entry)
	{
		return Entry.PlayerState == PlayerState;
	});
}

FTBLobbyReadyEntry* ATBLobbyManager::FindEntry(const APlayerState* PlayerState)
{
	return Entries.FindByPredicate([PlayerState](const FTBLobbyReadyEntry& Entry)
	{
		return Entry.PlayerState == PlayerState;
	});
}

// 준비 / 시작 ----------------------

void ATBLobbyManager::ToggleReady(APlayerState* PlayerState)
{
	// 맵 이동이 시작된 뒤에는 상태를 바꾸지 않음
	if (!HasAuthority() || bTravelStarted)
	{
		return;
	}

	FTBLobbyReadyEntry* Entry = FindEntry(PlayerState);
	if (!Entry)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TBLobby] ToggleReady: 목록에 없는 플레이어 %s"), *GetNameSafe(PlayerState));
		return;
	}

	Entry->bReady = !Entry->bReady;
	UE_LOG(LogTemp, Log, TEXT("[TBLobby] %s → %s"),
		*PlayerState->GetPlayerName(), Entry->bReady ? TEXT("Ready") : TEXT("Not Ready"));

	NotifyChanged();
}

int32 ATBLobbyManager::GetReadyCount() const
{
	int32 Count = 0;
	for (const FTBLobbyReadyEntry& Entry : Entries)
	{
		if (Entry.bReady)
		{
			++Count;
		}
	}
	return Count;
}

bool ATBLobbyManager::CanStartGame() const
{
	// 최소 인원 충족 + 전원 준비 (호스트 포함)
	return Entries.Num() >= MinPlayersToStart
		&& GetReadyCount() == Entries.Num();
}

void ATBLobbyManager::StartGame()
{
	// 서버만 / 한 번만 / 조건 재검사 (NPC 쪽 검사를 믿지 않고 여기서도 확인)
	if (!HasAuthority() || bTravelStarted || !CanStartGame())
	{
		return;
	}

	bTravelStarted = true;

	// 넘어갈 인원을 같이 전달 → Battlefield의 LevelManager가 이 인원을 기다림
	const FString TravelURL = FString::Printf(TEXT("%s?listen?ExpectedPlayers=%d"),
		*BattlefieldMapPath, Entries.Num());

	UE_LOG(LogTemp, Log, TEXT("[TBLobby] ServerTravel: %s"), *TravelURL);
	GetWorld()->ServerTravel(TravelURL);
}

// 알림 ----------------------

void ATBLobbyManager::NotifyChanged()
{
	// 서버(호스트)는 OnRep을 받지 않으므로 직접 호출
	OnRep_Entries();
	// 다음 복제 주기를 기다리지 않고 바로 보냄
	ForceNetUpdate();
}

void ATBLobbyManager::OnRep_Entries()
{
	OnLobbyChangedEvent.Broadcast();
}