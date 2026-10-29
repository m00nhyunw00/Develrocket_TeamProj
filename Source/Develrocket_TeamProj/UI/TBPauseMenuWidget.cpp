#include "UI/TBPauseMenuWidget.h"
#include "Components/Button.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"


void UTBPauseMenuWidget::OpenPauseMenu()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}
	
	SetVisibility(ESlateVisibility::Visible);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(TakeWidget());
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);
}

void UTBPauseMenuWidget::ClosePauseMenu()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}
	
	SetVisibility(ESlateVisibility::Collapsed);
	FInputModeGameOnly InputMode;
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(false);
}

void UTBPauseMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	
	if (ResumeButton)
	{
		ResumeButton->OnClicked.AddDynamic(this, &UTBPauseMenuWidget::OnResumeButtonClicked);
	}
	if (GuideButton)
	{
		GuideButton->OnClicked.AddDynamic(this, &UTBPauseMenuWidget::OnGuideButtonClicked);
	}
	if (QuitButton)
	{
		QuitButton->OnClicked.AddDynamic(this, &UTBPauseMenuWidget::OnQuitButtonClicked);
	}
}

void UTBPauseMenuWidget::OnResumeButtonClicked()
{
	ClosePauseMenu();
}

void UTBPauseMenuWidget::OnGuideButtonClicked()
{
	UE_LOG(LogTemp, Log, TEXT("Guide Clicked"));
}

void UTBPauseMenuWidget::OnQuitButtonClicked()
{
	UKismetSystemLibrary::QuitGame(GetWorld(), GetOwningPlayer(), EQuitPreference::Quit, false);
}