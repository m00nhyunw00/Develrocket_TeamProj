#include "UI/TBOverheadBloodBarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Engine/World.h"

namespace
{
	constexpr double DamageHoldSeconds = 0.2;
	constexpr double DamageDrainSeconds = 0.55;
}

void UTBOverheadBloodBarWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!BloodBar && WidgetTree && !WidgetTree->RootWidget)
	{
		UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>();
		WidgetTree->RootWidget = Layers;
		DamageBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("DamageBar"));
		BloodBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("BloodBar"));

		FProgressBarStyle Style = BloodBar->GetWidgetStyle();
		Style.SetBackgroundImage(FSlateColorBrush(FLinearColor(0.025f, 0.025f, 0.025f)));
		Style.SetFillImage(FSlateColorBrush(FLinearColor::White));
		DamageBar->SetWidgetStyle(Style);
		BloodBar->SetWidgetStyle(Style);

		for (UProgressBar* Bar : {DamageBar.Get(), BloodBar.Get()})
		{
			UOverlaySlot* LayerSlot = Layers->AddChildToOverlay(Bar);
			LayerSlot->SetHorizontalAlignment(HAlign_Fill);
			LayerSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}

	if (BloodBar)
	{
		BloodBar->SetFillColorAndOpacity(FLinearColor(1.0f, 0.72f, 0.035f));
	}
	if (BloodBar && DamageBar)
	{
		FProgressBarStyle ForegroundStyle = BloodBar->GetWidgetStyle();
		ForegroundStyle.SetBackgroundImage(FSlateNoResource());
		BloodBar->SetWidgetStyle(ForegroundStyle);

		DamageBar->SetBarFillType(BloodBar->GetBarFillType());
		DamageBar->SetBarFillStyle(BloodBar->GetBarFillStyle());
		DamageBar->SetBorderPadding(BloodBar->GetBorderPadding());
		DamageBar->SetFillColorAndOpacity(FLinearColor(0.8f, 0.025f, 0.025f));
	}
}

void UTBOverheadBloodBarWidget::SetBlood(float Blood, float MaxBlood)
{
	const float NewPercent = MaxBlood > 0.0f ? FMath::Clamp(Blood / MaxBlood, 0.0f, 1.0f) : 0.0f;
	const UWorld* World = GetWorld();
	if (!World || !DamageBar || LastMaxBlood != MaxBlood || NewPercent > BloodPercent)
	{
		DamageStartPercent = DamagePercent = NewPercent;
		if (DamageBar) DamageBar->SetPercent(NewPercent);
	}
	else if (NewPercent < BloodPercent)
	{
		const double Now = World->GetTimeSeconds();
		UpdateDamageBar(Now);
		DamageStartPercent = DamagePercent = FMath::Max(DamagePercent, BloodPercent);
		DamageStartTime = Now + DamageHoldSeconds;
		DamageBar->SetPercent(DamagePercent);
	}

	LastMaxBlood = MaxBlood;
	BloodPercent = NewPercent;
	if (BloodBar) BloodBar->SetPercent(NewPercent);
}

void UTBOverheadBloodBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (const UWorld* World = GetWorld())
	{
		UpdateDamageBar(World->GetTimeSeconds());
	}
}

void UTBOverheadBloodBarWidget::UpdateDamageBar(double Now)
{
	if (!DamageBar || DamagePercent <= BloodPercent || Now <= DamageStartTime) return;

	const float Alpha = static_cast<float>(FMath::Clamp((Now - DamageStartTime) / DamageDrainSeconds, 0.0, 1.0));
	DamagePercent = FMath::Lerp(DamageStartPercent, BloodPercent, Alpha);
	DamageBar->SetPercent(DamagePercent);
}
