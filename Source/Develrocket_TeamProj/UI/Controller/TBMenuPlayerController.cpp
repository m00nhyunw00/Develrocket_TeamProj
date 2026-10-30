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
