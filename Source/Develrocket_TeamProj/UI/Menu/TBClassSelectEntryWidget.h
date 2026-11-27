#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TBClassSelectEntryWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UTBCharacterClassData;

DECLARE_DELEGATE_OneParam(FOnTBClassEntryClicked, const UTBCharacterClassData*);

// 직업 선택 팝업의 버튼 한 개 (자기 직업 데이터를 들고 있음)
UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBClassSelectEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(const UTBCharacterClassData* InClassData);

	FOnTBClassEntryClicked OnEntryClicked;

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SelectButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	// 아이콘은 없어도 동작
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

private:
	// UButton::OnClicked는 파라미터를 못 넘기므로 버튼마다 자기 데이터를 보관
	UFUNCTION()
	void HandleClicked();

	UPROPERTY(Transient)
	TObjectPtr<const UTBCharacterClassData> ClassData;
};
