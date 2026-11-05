#include "UI/TBAmmoWidget.h"
#include "Components/TextBlock.h"

void UTBAmmoWidget::SetAmmo(int32 CurrentAmmo, int32 AmmoAtLastReload, int32 MagazineSize, bool bCanReload)
{
	if (AmmoText)
	{
		const FString AmmoString = FString::Printf(TEXT("%d / %d"), CurrentAmmo, AmmoAtLastReload);

		AmmoText->SetText(FText::FromString(AmmoString));

		// 탄약 부족 색상은 탄창 최대 용량 기준
		const bool bLowAmmo = MagazineSize > 0 && static_cast<float>(CurrentAmmo) / MagazineSize <= LowAmmoRatio;

		AmmoText->SetColorAndOpacity(bLowAmmo ? AmmoLowColor : AmmoNormalColor);
	}

	if (ReloadText)
	{
		ReloadText->SetText(FText::FromString(bCanReload ? TEXT("RELOAD") : TEXT("NO AMMO")));

		ReloadText->SetVisibility(CurrentAmmo <= 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}
