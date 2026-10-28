// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/TBPlayerHUDWidget.h"

#include "UI/TBLowHealthOverlayWidget.h"
#include "UI/TBCrosshairWidget.h"
#include "UI/TBHealthBarWidget.h"
#include "UI/TBAmmoWidget.h"
#include "UI/TBHitMarkerWidget.h"

void UTBPlayerHUDWidget::SetHealth(float CurrentHealth, float MaxHealth)
{
	if (HealthBarWidget)
	{
		HealthBarWidget->SetHealth(CurrentHealth, MaxHealth);
	}
	
	if (LowHealthOverlayWidget)
	{
		LowHealthOverlayWidget->SetHealth(CurrentHealth, MaxHealth);
	}	
}

void UTBPlayerHUDWidget::SetCrosshairSpread(float Spread)
{
	if (!CrosshairWidget)
	{
		return;
	}
	CrosshairWidget->SetCrosshairSpread(Spread);		
}

void UTBPlayerHUDWidget::SetAiming(bool bIsAiming)
{
	if (!CrosshairWidget)
	{
		return;
	}
	CrosshairWidget->SetAiming(bIsAiming);
}

void UTBPlayerHUDWidget::SetShowHitMarker(bool bIsKill)
{
	if (!HitMarkerWidget)
	{
		return;
	}
	HitMarkerWidget->SetShowHitMarker(bIsKill);
}

void UTBPlayerHUDWidget::SetAmmo(int32 CurrentAmmo, int32 ReserveAmmo, int32 MagazineSize)
{
	if (!AmmoWidget)
	{
		return;
	}
	
	AmmoWidget->SetAmmo(CurrentAmmo, ReserveAmmo, MagazineSize);
}
