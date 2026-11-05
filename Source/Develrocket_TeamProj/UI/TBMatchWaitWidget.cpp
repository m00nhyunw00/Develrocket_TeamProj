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
    }

    Super::NativeDestruct();
}

void UTBMatchWaitWidget::OnStartClicked()
{
    if (ATBLevelManager* Manager = LevelManager.Get())
    {
        StartButton->SetIsEnabled(false);   // 연타 방지
        Manager->StartMatch();              // 호스트 = 서버라서 직접 호출 가능
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

    if (!bReady)
    {
        StatusText->SetText(FText::FromString(TEXT("LOADING HUNT...")));
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