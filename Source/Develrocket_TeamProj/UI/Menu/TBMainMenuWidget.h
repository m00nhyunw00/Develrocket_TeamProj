#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TBMainMenuWidget.generated.h"

class UButton;
class UWidgetAnimation;
class UWidgetSwitcher;

UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UWidgetSwitcher> MenuSwitcher;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> HostButton;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> JoinButton;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> QuitButton;
	
	UPROPERTY(Transient, meta=(BindWidgetAnim))
	TObjectPtr<UWidgetAnimation> PressBlinkAnim;
	
	UPROPERTY(EditDefaultsOnly, Category="Menu")
	FName LobbyLevelName = TEXT("L_Lobby");
	
	UPROPERTY(EditDefaultsOnly, Category="Menu")
	FString JoinAddress = TEXT("127.0.0.1");
	
private:
	void ShowMenuButtons();
	
	UFUNCTION()
	void OnHostButtonClicked();
	UFUNCTION()
	void OnJoinButtonClicked();
	UFUNCTION()
	void OnQuitButtonClicked();
};