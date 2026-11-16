#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TBMatchWaitWidget.generated.h"

class UButton;
class UTextBlock;
class ATBLevelManager;

UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBMatchWaitWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> StatusText;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> StartButton;
	
private:
	UFUNCTION()
	void OnStartClicked();
	
	UFUNCTION()
	void HandleCityReady();
	void HandleMatchStarted();
	
	void RefreshUI();
	bool IsHost() const;
	
	TWeakObjectPtr<ATBLevelManager> LevelManager;
};