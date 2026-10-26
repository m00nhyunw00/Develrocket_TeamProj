// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/PlayerHUD.h"

#include "Blueprint/UserWidget.h"
#include "UI/PlayerHUDWidget.h"

void APlayerHUD::TestDamage(float Amount)
{
	TestHealth = FMath::Clamp(TestHealth - Amount, 0.f, TestMaxHealth);
	
	if (HUDWidget)
	{
		HUDWidget->SetHealth(TestHealth, TestMaxHealth);
	}
}

void APlayerHUD::TestHeal(float Amount)
{
	TestHealth = FMath::Clamp(TestHealth + Amount, 0.f, TestMaxHealth);
	
	if (HUDWidget)
	{
		HUDWidget->SetHealth(TestHealth, TestMaxHealth);
	}
}

void APlayerHUD::TestSpread(float Spread)
{
	if (HUDWidget)
	{
		HUDWidget->SetCrosshairSpread(Spread);
	}
}

void APlayerHUD::TestAim(bool bIsAiming)
{
	if (HUDWidget)
	{
		HUDWidget->SetAiming(bIsAiming);
	}
}

void APlayerHUD::TestHit(bool bIsKill)
{
	if (HUDWidget)
	{
		HUDWidget->SetShowHitMarker(bIsKill);
	}
}

void APlayerHUD::TestSetAmmo(int32 Current, int32 Reserve)
{
	TestAmmo = Current;
	TestReserveAmmo = Reserve;
	
	if (HUDWidget)
	{
		HUDWidget->SetAmmo(TestAmmo, TestReserveAmmo, TestMagazineSize);
	}
}

void APlayerHUD::BeginPlay()
{
	Super::BeginPlay();
	
	if (!HUDWidgetClass)
	{
		return;
	}
	
	HUDWidget = CreateWidget<UPlayerHUDWidget>(GetOwningPlayerController(), HUDWidgetClass);
	if (HUDWidget)
	{
		HUDWidget->AddToViewport();
		HUDWidget->SetHealth(TestHealth, TestMaxHealth);
		HUDWidget->SetAmmo(TestAmmo, TestReserveAmmo, TestMagazineSize);
	}
}
