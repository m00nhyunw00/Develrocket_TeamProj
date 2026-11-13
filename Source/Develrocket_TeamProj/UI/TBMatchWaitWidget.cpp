#include "UI/TBMatchWaitWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Framework/TBLevelManager.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

void UTBMatchWaitWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (StartButton)
    {
        StartButton->OnClicked.AddDynamic(this, &UTBMatchWaitWidget::OnStartClicked);
    }
}

void UTBMatchWaitWidget::NativeConstruct()
{
    Super::NativeConstruct();

    LevelManager = Cast<ATBLevelManager>(
        UGameplayStatics::GetActorOfClass(this, ATBLevelManager::StaticClass()));

    ATBLevelManager* Manager = LevelManager.Get();
    if (!Manager)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TBMatchWait] LevelManager not found"));
        return;
    }

    // 현재 상태 먼저 반영
    RefreshUI();

    // 늦게 들어온 클라이언트: 이미 시작됨
    if (Manager->IsMatchStarted())
    {
        HandleMatchStarted();
        return;
    }

    // 구독 (위젯 = 받는 쪽)
    Manager->OnLocalCityReadyEvent.AddUObject(this, &UTBMatchWaitWidget::HandleCityReady);
    Manager->OnMatchStartedEvent.AddUObject(this, &UTBMatchWaitWidget::HandleMatchStarted);
	Manager->OnSpawnSearchChangedEvent.AddUObject(this, &UTBMatchWaitWidget::HandleCityReady);
    Manager->OnAutoStartModeChangedEvent.AddUObject(this, &UTBMatchWaitWidget::HandleCityReady);

    // 대기 중: 입력은 UI만, 커서 표시
    if (APlayerController* PC = GetOwningPlayer())
    {
        FInputModeUIOnly InputMode;
        InputMode.SetWidgetToFocus(TakeWidget());
        PC->SetInputMode(InputMode);
        PC->SetShowMouseCursor(true);
    }
}

void UTBMatchWaitWidget::NativeDestruct()
{
    // 나만 그만 듣기
    if (ATBLevelManager* Manager = LevelManager.Get())
    {
        Manager->OnLocalCityReadyEvent.RemoveAll(this);
        Manager->OnMatchStartedEvent.RemoveAll(this);
        Manager->OnAutoStartModeChangedEvent.RemoveAll(this);
		Manager->OnSpawnSearchChangedEvent.RemoveAll(this);
    }

    Super::NativeDestruct();
}

void UTBMatchWaitWidget::OnStartClicked()
{
    if (ATBLevelManager* Manager = LevelManager.Get())
    {
        StartButton->SetIsEnabled(false);   // 연타 방지
		if (!Manager->StartMatch())
		{
			StartButton->SetIsEnabled(true);
		}
    }
}

void UTBMatchWaitWidget::HandleCityReady()
{
    RefreshUI();
}

void UTBMatchWaitWidget::HandleMatchStarted()
{
    if (APlayerController* PC = GetOwningPlayer())
    {
        PC->SetInputMode(FInputModeGameOnly());
        PC->SetShowMouseCursor(false);
    }

    RemoveFromParent();
}

void UTBMatchWaitWidget::RefreshUI()
{
    ATBLevelManager* Manager = LevelManager.Get();
    const bool bReady = Manager && Manager->IsLocalCityReady();
	if (Manager && Manager->IsSearchingForSpawn())
	{
		StatusText->SetText(FText::FromString(TEXT("SEARCHING FOR SAFE SPAWN...")));
		StartButton->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

    if (!bReady)
    {
        StatusText->SetText(FText::FromString(TEXT("LOADING HUNT...")));
        StartButton->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }
    
    // 로비를 거쳐 온 매치는 금방 시작함, 나중에 실제 패키징에선 이렇게 될 예정
    if (Manager->IsAutoStartMode())
    {
        StatusText->SetText(FText::FromString(TEXT("WAITING FOR PLAYERS...")));
        StartButton->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    if (IsHost())
    {
        StatusText->SetText(FText::FromString(TEXT("HUNT READY")));
        StartButton->SetVisibility(ESlateVisibility::Visible);
        StartButton->SetIsEnabled(true);
    }
    else
    {
        StatusText->SetText(FText::FromString(TEXT("WAITING FOR HOST...")));
        StartButton->SetVisibility(ESlateVisibility::Collapsed);
    }
}

bool UTBMatchWaitWidget::IsHost() const
{
    APlayerController* PC = GetOwningPlayer();
    return PC && PC->HasAuthority();
}
