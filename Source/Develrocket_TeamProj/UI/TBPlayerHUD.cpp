// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/TBPlayerHUD.h"

#include "Blueprint/UserWidget.h"
#include "UI/TBPlayerHUDWidget.h"

void ATBPlayerHUD::TestDamage(float Amount)
{
	TestHealth = FMath::Clamp(TestHealth - Amount, 0.f, TestMaxHealth);
	
	if (HUDWidget)
	{
		HUDWidget->SetHealth(TestHealth, TestMaxHealth);
	}
}

void ATBPlayerHUD::TestHeal(float Amount)
{
	TestHealth = FMath::Clamp(TestHealth + Amount, 0.f, TestMaxHealth);
	
	if (HUDWidget)
	{
		HUDWidget->SetHealth(TestHealth, TestMaxHealth);
	}
}

void ATBPlayerHUD::TestSpread(float Spread)
{
	if (HUDWidget)
	{
		HUDWidget->SetCrosshairSpread(Spread);
	}
}

void ATBPlayerHUD::TestAim(bool bIsAiming)
{
	if (HUDWidget)
	{
		HUDWidget->SetAiming(bIsAiming);
	}
}

void ATBPlayerHUD::TestHit(bool bIsKill)
{
	if (HUDWidget)
	{
		HUDWidget->SetShowHitMarker(bIsKill);
	}
}

void ATBPlayerHUD::TestSetAmmo(int32 Current, int32 Reserve)
{
	TestAmmo = Current;
	TestReserveAmmo = Reserve;
	
	if (HUDWidget)
	{
		HUDWidget->SetAmmo(TestAmmo, TestReserveAmmo, TestMagazineSize);
	}
}

void ATBPlayerHUD::TestSkill(int32 Index, float Duration)
{
	if (HUDWidget)
	{
		HUDWidget->StartSkillCooldown(Index, Duration);
	}
}

void ATBPlayerHUD::BeginPlay()
{
	Super::BeginPlay();
	
	if (!HUDWidgetClass)
	{
		return;
	}
	
	HUDWidget = CreateWidget<UTBPlayerHUDWidget>(GetOwningPlayerController(), HUDWidgetClass);
	if (HUDWidget)
	{
		HUDWidget->AddToViewport();
		HUDWidget->SetHealth(TestHealth, TestMaxHealth);
		HUDWidget->SetAmmo(TestAmmo, TestReserveAmmo, TestMagazineSize);
		HUDWidget->SetSkill(0, nullptr, FText::FromString(TEXT("Q")));
		HUDWidget->SetSkill(1,nullptr, FText::FromString(TEXT("E")));
	}
}
