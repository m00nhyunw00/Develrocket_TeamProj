// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/PlayerHUDWidget.h"

#include "UI/LowHealthOverlayWidget.h"
#include "UI/CrosshairWidget.h"
#include "UI/HealthBarWidget.h"
#include "UI/AmmoWidget.h"
#include "UI/HitMarkerWidget.h"

void UPlayerHUDWidget::SetHealth(float CurrentHealth, float MaxHealth)
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

void UPlayerHUDWidget::SetCrosshairSpread(float Spread)
{
	if (!CrosshairWidget)
	{
		return;
	}
	CrosshairWidget->SetCrosshairSpread(Spread);		
}

void UPlayerHUDWidget::SetAiming(bool bIsAiming)
{
	if (!CrosshairWidget)
	{
		return;
	}
	CrosshairWidget->SetAiming(bIsAiming);
}

void UPlayerHUDWidget::SetShowHitMarker(bool bIsKill)
{
	if (!HitMarkerWidget)
	{
		return;
	}
	HitMarkerWidget->SetShowHitMarker(bIsKill);
}

void UPlayerHUDWidget::SetAmmo(int32 CurrentAmmo, int32 ReserveAmmo, int32 MagazineSize)
{
	if (!AmmoWidget)
	{
		return;
	}
	
	AmmoWidget->SetAmmo(CurrentAmmo, ReserveAmmo, MagazineSize);
}
