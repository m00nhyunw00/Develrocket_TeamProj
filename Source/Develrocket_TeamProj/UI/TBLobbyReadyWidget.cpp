#include "UI/TBLobbyReadyWidget.h"

#include "Components/TextBlock.h"
#include "Framework/TBLobbyManager.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"

void UTBLobbyReadyWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 표시 전용: 마우스 입력을 막지 않음
	SetVisibility(ESlateVisibility::HitTestInvisible);

	// 배치 액터라 클라에도 맵 로드 시점부터 존
	LobbyManager = Cast<ATBLobbyManager>(UGameplayStatics::GetActorOfClass(this, ATBLobbyManager::StaticClass()));

	ATBLobbyManager* Manager = LobbyManager.Get();
	if (!Manager)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TBLobbyReady] LobbyManager not found"));
		return;
	}

	Manager->OnLobbyChangedEvent.AddUObject(this, &ThisClass::HandleLobbyChanged);
	Refresh();
}

void UTBLobbyReadyWidget::NativeDestruct()
{
	if (ATBLobbyManager* Manager = LobbyManager.Get())
	{
		Manager->OnLobbyChangedEvent.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UTBLobbyReadyWidget::HandleLobbyChanged()
{
	Refresh();
}

void UTBLobbyReadyWidget::Refresh()
{
	const ATBLobbyManager* Manager = LobbyManager.Get();
	if (!Manager || !PlayerListText || !SummaryText)
	{
		return;
	}

	FString ListString;
	int32 Number = 0;

	for (const FTBLobbyReadyEntry& Entry : Manager->GetEntries())
	{
		// 클라: PlayerState가 아직 도착하지 않은 항목은 건너뜀 (도착하면 OnRep이 다시 옴)
		const APlayerState* PlayerState = Entry.PlayerState;
		if (!PlayerState)
		{
			continue;
		}

		++Number;
		ListString += FString::Printf(TEXT("%d. %s    %s\n"),
			Number,
			*PlayerState->GetPlayerName(),
			Entry.bReady ? TEXT("준비완료") : TEXT("대기 중"));
	}

	PlayerListText->SetText(FText::FromString(ListString));

	SummaryText->SetText(FText::FromString(FString::Printf(TEXT("준비 %d / %d   (최소 %d명)"),
		Manager->GetReadyCount(),
		Manager->GetEntries().Num(),
		Manager->GetMinPlayersToStart())));
}