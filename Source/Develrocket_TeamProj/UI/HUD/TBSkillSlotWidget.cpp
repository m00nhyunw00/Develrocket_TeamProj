#include "UI/HUD/TBSkillSlotWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

void UTBSkillSlotWidget::SetIcon(UTexture2D* Icon)
{
	if (!Icon || !SkillIcon)
	{
		return;
	}
	SkillIcon->SetBrushFromTexture(Icon);		
}

void UTBSkillSlotWidget::SetKey(const FText& Key)
{
	if (!KeyText)
	{
		return;
	}
	
	KeyText->SetText(Key);
}

void UTBSkillSlotWidget::SetCooldownRemaining(float RemainingSeconds)
{
	const int32 Seconds = FMath::CeilToInt(FMath::Max(0.0f, RemainingSeconds));
	if (DisplayedCooldownSeconds == Seconds)
	{
		return;
	}

	const bool bWasReady = DisplayedCooldownSeconds == 0;
	const bool bIsReady = Seconds == 0;
	DisplayedCooldownSeconds = Seconds;
	if (bWasReady != bIsReady)
	{
		UpdateReadyVisual(bIsReady);
	}
	if (CooldownText && !bIsReady)
	{
		CooldownText->SetText(FText::AsNumber(Seconds));
	}
}

void UTBSkillSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UpdateReadyVisual(DisplayedCooldownSeconds == 0);
}

void UTBSkillSlotWidget::UpdateReadyVisual(bool bReady)
{
	//프레임 교체
	UTexture2D* Frame = bReady ? ReadyFrame : CooldownFrame;
	if (SkillFrame && Frame)
	{
		SkillFrame->SetBrushFromTexture(Frame);
	}
	
	//아이콘 밝기
	if (SkillIcon)
	{
		SkillIcon->SetColorAndOpacity(bReady ? FLinearColor(1.f, 1.f, 1.f, 1.f) : 
			FLinearColor(0.35f, 0.35f, 0.35f, 1.f));
	}
	
	// 쿨 다운 숫자
	if (CooldownText)
	{
		CooldownText->SetVisibility(bReady ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
	}
}


