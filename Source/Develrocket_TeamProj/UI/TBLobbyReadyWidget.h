#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TBLobbyReadyWidget.generated.h"

class UTextBlock;
class ATBLobbyManager;

UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBLobbyReadyWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// "1. 이름    준비완료" 여러 줄
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PlayerListText;

	// "준비 n / m   (최소 k명)"
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SummaryText;

private:
	void HandleLobbyChanged();
	void Refresh();

	TWeakObjectPtr<ATBLobbyManager> LobbyManager;
};