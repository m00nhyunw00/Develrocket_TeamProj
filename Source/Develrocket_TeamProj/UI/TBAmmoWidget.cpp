#include "UI/TBAmmoWidget.h"
#include "Components/TextBlock.h"

void UTBAmmoWidget::SetAmmo(int32 CurrentAmmo, int32 ReserveAmmo, int32 MagazineSize)
{
	if (!AmmoText || !ReloadText)
	{
		return;
	}
	
	const FString AmmoString = FString::Printf(TEXT("%d / %d"), CurrentAmmo, ReserveAmmo);
	AmmoText->SetText(FText::FromString(AmmoString));
	
	// 부족 판단 (탄창 크기가 0 이하면 나눌 수 없으니 부족 아님)
	const bool bLow = MagazineSize > 0 && (float)CurrentAmmo / MagazineSize <= LowAmmoRatio;
	AmmoText->SetColorAndOpacity(bLow ? AmmoLowColor : AmmoNormalColor);
	
	// 0발일 때만 문구 표시
	if (CurrentAmmo == 0)
	{
		ReloadText->SetText(FText::FromString(ReserveAmmo > 0 ? TEXT("RELOAD") : TEXT("NO AMMO")));
		ReloadText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		ReloadText->SetVisibility(ESlateVisibility::Collapsed);
	}
	
}
