#include "UI/CrosshairWidget.h"
#include "Components/Image.h"

void UCrosshairWidget::SetCrosshairSpread(float Spread)
{
	TargetSpread = FMath::Clamp(Spread, 0.f, 1.f);
}

void UCrosshairWidget::SetAiming(bool bIsAiming)
{
	if (bIsAiming == true)
	{
		this->SetRenderOpacity(0);
	}
	else
	{
		this->SetRenderOpacity(1);
	}
}

void UCrosshairWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyCrosshairSpread(CurrentSpread);
}

void UCrosshairWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	// 이미 목표에 거의 도착했으면 아무것도 안 함(매 프레임 배치하면 낭비)
	if (FMath::IsNearlyEqual(TargetSpread, CurrentSpread, 0.001f))
	{
		return;
	}
	
	CurrentSpread = FMath::FInterpTo(CurrentSpread, TargetSpread, InDeltaTime, CrosshairInterpSpeed);
	ApplyCrosshairSpread(CurrentSpread);
}

void UCrosshairWidget::ApplyCrosshairSpread(float Spread)
{
	if (!CrosshairTop || !CrosshairBottom || !CrosshairLeft || !CrosshairRight)
	{
		return;
	}
	
	const float Gap = FMath::Lerp(CrosshairMinGap, CrosshairMaxGap, FMath::Clamp(Spread, 0.f, 1.f));
	const float HalfBlade = CrosshairTop->GetBrush().ImageSize.Y*0.5f;
	const float Offset = Gap + HalfBlade;
	
	CrosshairTop->SetRenderTranslation(FVector2D(0.f, -Offset));
	CrosshairBottom->SetRenderTranslation(FVector2D(0.f, Offset));
	CrosshairLeft->SetRenderTranslation(FVector2D(-Offset, 0.f));
	CrosshairRight->SetRenderTranslation(FVector2D(Offset, 0.f));
}
