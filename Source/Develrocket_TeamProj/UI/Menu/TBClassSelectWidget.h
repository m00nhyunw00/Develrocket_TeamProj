	#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/TBClassChangeTypes.h"
#include "TBClassSelectWidget.generated.h"

class ATBPlayerState;
class UButton;
class UPanelWidget;
class UTBCharacterClassData;
class UTBClassSelectEntryWidget;

// 로비 직업 선택 팝업 (LobbyManager의 AvailableClasses로 카드 생성)
// - 첫 입장: 직업을 고르기 전엔 닫기 버튼 숨김
// - P로 다시 열기: 현재 직업 카드는 Disabled, 닫기 허용
// - 카드 클릭 후 서버 결과(승인)를 받은 뒤에 닫음
UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBClassSelectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void OpenSelect();
	void CloseSelect();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	// 직업 카드가 들어갈 칸 (HorizontalBox / VerticalBox / WrapBox)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> EntryBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UTBClassSelectEntryWidget> EntryWidgetClass;

private:
	void BuildEntries();

	// 카드 상태 갱신: 현재 직업 Disabled, 첫 선택 전엔 닫기 숨김
	void RefreshEntries();
	void SetAllEntriesSelectable(bool bSelectable);

	void HandleEntryClicked(const UTBCharacterClassData* ClassData);

	UFUNCTION()
	void HandleCloseClicked();

	// 입장 직후 클라엔 PlayerState가 없을 수 있어 열 때·클릭할 때 바인딩 시도
	void BindClassChangeResult();
	void HandleClassChangeResult(ETBClassChangeResult Result);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTBClassSelectEntryWidget>> EntryWidgets;

	TWeakObjectPtr<ATBPlayerState> BoundPlayerState;
	FDelegateHandle ClassChangeResultHandle;
};
