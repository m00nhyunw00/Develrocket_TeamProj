#include "UI/Controller/TBMenuPlayerController.h"
#include "UI/Menu/TBMainMenuWidget.h"

void ATBMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	if (!IsLocalController())
	{
		return;
	}
	
	if (MainMenuClass)
	{
		MainMenuWidget = CreateWidget<UTBMainMenuWidget>(this, MainMenuClass);
		if (MainMenuWidget)
		{
			MainMenuWidget->AddToViewport();
			
			FInputModeUIOnly InputMode;
			InputMode.SetWidgetToFocus(MainMenuWidget->TakeWidget());
			SetInputMode(InputMode);
			SetShowMouseCursor(true);
		}
	}		
}

void ATBMenuPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsLocalController())
        {
            // 메뉴에서 걸어둔 UI Only를 해제하고 게임 입력으로 복구
            SetInputMode(FInputModeGameOnly());
            SetShowMouseCursor(false);
        }
	
	Super::EndPlay(EndPlayReason);
}
