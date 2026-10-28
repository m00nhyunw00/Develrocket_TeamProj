#include "UI/TBSkillSlotWidget.h"
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

void UTBSkillSlotWidget::StartCooldown(float Duration)
{
	RemainingTime = Duration;
	bIsSkillReady = false;
	UpdateReadyVisual(bIsSkillReady);
}

void UTBSkillSlotWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	if (bIsSkillReady)
	{
		return;
	}
	
	RemainingTime -= InDeltaTime;
	
	if (RemainingTime <= 0.f)
	{
		bIsSkillReady = true;
		UpdateReadyVisual(true);
		return;
	}
	
	if (CooldownText)
	{
		CooldownText->SetText(FText::AsNumber(FMath::CeilToInt(RemainingTime)));
	}
}

void UTBSkillSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UpdateReadyVisual(true);
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


