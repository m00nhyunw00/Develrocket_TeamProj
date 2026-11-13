#include "UI/TBOverheadBloodBarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/ProgressBar.h"

void UTBOverheadBloodBarWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!BloodBar && WidgetTree && !WidgetTree->RootWidget)
	{
		BloodBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("BloodBar"));
		WidgetTree->RootWidget = BloodBar;
		BloodBar->SetFillColorAndOpacity(FLinearColor(0.75f, 0.03f, 0.03f, 1.0f));
	}
}

void UTBOverheadBloodBarWidget::SetBlood(float Blood, float MaxBlood)
{
	if (BloodBar) BloodBar->SetPercent(MaxBlood > 0.0f ? FMath::Clamp(Blood / MaxBlood, 0.0f, 1.0f) : 0.0f);
}
