#include "UI/Menu/TBMainMenuWidget.h"
#include "Components/Button.h"
#include "Components/WidgetSwitcher.h"
#include "Animation/WidgetAnimation.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Network/TBSessionSubsystem.h"
#include "UI/Menu/TBSessionListWidget.h"

void UTBMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	
	SetIsFocusable(true);
	
	if (HostButton)
	{
		HostButton->OnClicked.AddDynamic(this, &UTBMainMenuWidget::OnHostButtonClicked);
	}
	if (JoinButton)
	{
		JoinButton->OnClicked.AddDynamic(this, &UTBMainMenuWidget::OnJoinButtonClicked);
	}
	if (QuitButton)
	{
		QuitButton->OnClicked.AddDynamic(this, &UTBMainMenuWidget::OnQuitButtonClicked);
	}
	if (SessionList)
	{
		SessionList->OnBackRequested.AddUObject(this, &UTBMainMenuWidget::HandleSessionListBack);
	}
}

void UTBMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (PressBlinkAnim)
	{
		PlayAnimation(PressBlinkAnim, 0.f, 0);
	}
	SetKeyboardFocus();
}

FReply UTBMainMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (MenuSwitcher && MenuSwitcher->GetActiveWidgetIndex()==0)
	{
		ShowMenuButtons();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UTBMainMenuWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (MenuSwitcher && MenuSwitcher->GetActiveWidgetIndex()==0)
	{
		ShowMenuButtons();
		return FReply::Handled();
	}
	
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UTBMainMenuWidget::ShowMenuButtons()
{
	if (MenuSwitcher)
	{
		MenuSwitcher->SetActiveWidgetIndex(1);
	}
	if (PressBlinkAnim)
	{
		StopAnimation(PressBlinkAnim);
	}
}

void UTBMainMenuWidget::OnHostButtonClicked()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTBSessionSubsystem* Session = GI->GetSubsystem<UTBSessionSubsystem>())
		{
			Session->HostSession(MaxPlayers);
		}
	}
}

void UTBMainMenuWidget::OnJoinButtonClicked()
{
	if (MenuSwitcher && SessionList)
	{
		MenuSwitcher->SetActiveWidgetIndex(2);   // 목록 화면
		SessionList->StartSearch();
	}
}

void UTBMainMenuWidget::OnQuitButtonClicked()
{
	UKismetSystemLibrary::QuitGame(GetWorld(), GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UTBMainMenuWidget::HandleSessionListBack()
{
	if (MenuSwitcher)
	{
		MenuSwitcher->SetActiveWidgetIndex(1);   // 버튼 화면
	}
}