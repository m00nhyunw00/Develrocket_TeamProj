#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TBPauseMenuWidget.generated.h"

class UButton;

UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void OpenPauseMenu();
	void ClosePauseMenu();
	
protected:
	virtual void NativeOnInitialized() override;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> ResumeButton;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> GuideButton;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> QuitButton;
	
private:
	UFUNCTION()
	void OnResumeButtonClicked();
	UFUNCTION()
	void OnGuideButtonClicked();
	UFUNCTION()
	void OnQuitButtonClicked();
};