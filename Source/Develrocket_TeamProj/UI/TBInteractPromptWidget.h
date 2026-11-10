#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TBInteractPromptWidget.generated.h"

class UTextBlock;
class UWidgetAnimation;

UCLASS( Abstract )
class DEVELROCKET_TEAMPROJ_API UTBInteractPromptWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	// 글 붙이기: 키 표시 + 행동문구
	void ShowPrompt(const FText& Key, const FText& Prompt);
	void HidePrompt();
	
protected:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> InputText;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> ActText;
	
	UPROPERTY(Transient, meta=(BindWidgetAnim))
	TObjectPtr<UWidgetAnimation> Anim_Open;
};