#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TBClassSelectWidget.generated.h"

class UButton;
class UPanelWidget;
class UTBCharacterClassData;
class UTBClassSelectEntryWidget;

// 로비 직업 선택 팝업 (LobbyManager의 AvailableClasses로 버튼 생성)
UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBClassSelectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void OpenSelect();
	void CloseSelect();

protected:
	virtual void NativeOnInitialized() override;

	// 직업 버튼이 들어갈 칸 (HorizontalBox / VerticalBox / WrapBox)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> EntryBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UTBClassSelectEntryWidget> EntryWidgetClass;

private:
	void BuildEntries();
	void HandleEntryClicked(const UTBCharacterClassData* ClassData);

	UFUNCTION()
	void HandleCloseClicked();
};
