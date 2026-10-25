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
		HUDWidget->SetAmmo(TestAmmo, TestReserveAmmo);
	}
}
