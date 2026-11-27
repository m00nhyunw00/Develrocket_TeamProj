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

	// 현재 직업이면 false → 버튼 Disabled (안의 이미지·글자도 엔진이 흐리게 그림)
	void SetSelectable(bool bSelectable);

	const UTBCharacterClassData* GetClassData() const { return ClassData.Get(); }

	FOnTBClassEntryClicked OnEntryClicked;

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SelectButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	// 카드 그림 (DA의 Portrait). 없어도 동작
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> PortraitImage;

private:
	// UButton::OnClicked는 파라미터를 못 넘기므로 버튼마다 자기 데이터를 보관
	UFUNCTION()
	void HandleClicked();

	UPROPERTY(Transient)
	TObjectPtr<const UTBCharacterClassData> ClassData;
};
