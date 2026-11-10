#include "UI/TBInteractPromptWIdget.h"

#include "Components/TextBlock.h"
#include "Animation/WidgetAnimation.h"

void UTBInteractPromptWidget::ShowPrompt(const FText& Key, const FText& Prompt)
{
	if (!InputText || !ActText)
	{
		return;
	}
	
	ActText->SetText(Prompt);
	InputText->SetText(Key);
	
	SetVisibility(ESlateVisibility::HitTestInvisible);
	
	if (Anim_Open)
	{
		PlayAnimation(Anim_Open);
	}
	
}

void UTBInteractPromptWidget::HidePrompt()
{
	SetVisibility(ESlateVisibility::Collapsed);
}
