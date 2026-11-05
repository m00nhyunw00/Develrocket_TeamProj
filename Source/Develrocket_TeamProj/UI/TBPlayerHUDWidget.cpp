#include "UI/TBPlayerHUDWidget.h"

#include "TBSkillBarWidget.h"
#include "UI/TBLowHealthOverlayWidget.h"
#include "UI/TBCrosshairWidget.h"
#include "UI/TBHealthBarWidget.h"
#include "UI/TBAmmoWidget.h"
#include "UI/TBHitMarkerWidget.h"

void UTBPlayerHUDWidget::SetHealth(float CurrentHealth, float MaxHealth)
{
	if (HealthBar)
	{
		HealthBar->SetHealth(CurrentHealth, MaxHealth);
	}
	
	if (LowHealthOverlay)
	{
		LowHealthOverlay->SetHealth(CurrentHealth, MaxHealth);
	}	
}

void UTBPlayerHUDWidget::SetCrosshairSpread(float Spread)
{
	if (!Crosshair)
	{
		return;
	}
	Crosshair->SetCrosshairSpread(Spread);		
}

void UTBPlayerHUDWidget::NotifyShotFired()
{
	if (Crosshair)
	{
		Crosshair->NotifyShotFired();
	}
}

void UTBPlayerHUDWidget::SetAiming(bool bIsAiming)
{
	if (!Crosshair)
	{
		return;
	}
	Crosshair->SetAiming(bIsAiming);
}

void UTBPlayerHUDWidget::SetShowHitMarker(bool bIsKill)
{
	if (!HitMarker)
	{
		return;
	}
	HitMarker->SetShowHitMarker(bIsKill);
}

void UTBPlayerHUDWidget::SetAmmo(int32 CurrentAmmo, int32 AmmoAtLastReload, int32 MagazineSize, bool bCanReload)
{
	if (Ammo)
	{
		Ammo->SetAmmo(CurrentAmmo, AmmoAtLastReload, MagazineSize, bCanReload);
	}
}

void UTBPlayerHUDWidget::SetSkill(int32 Index, UTexture2D* Icon, const FText& Key)
{
	if (!SkillBar)
	{
		return;
	}
	
	SkillBar->SetSkill(Index, Icon, Key);
}

void UTBPlayerHUDWidget::StartSkillCooldown(int32 Index, float Duration)
{
	if (!SkillBar)
	{
		return;
	}
	SkillBar->StartSkillCooldown(Index, Duration);
}

