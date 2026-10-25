// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/PlayerHUDWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UPlayerHUDWidget::SetHealth(float CurrentHealth, float MaxHealth)
{
	if (!HealthBar || MaxHealth <= 0.f)
	{
		return;
	}
	
	const float Percent = FMath::Clamp(CurrentHealth / MaxHealth, 0.f, 1.f);
	HealthBar->SetPercent(Percent);
}

void UPlayerHUDWidget::SetAmmo(int32 CurrentAmmo, int32 ReserveAmmo)
{
	if (!AmmoText)
	{
		return;
	}
	
	const FString AmmoString = FString::Printf(TEXT("%d / %d"), CurrentAmmo, ReserveAmmo);
	AmmoText->SetText(FText::FromString(AmmoString));
}

void UPlayerHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UPlayerHUDWidget::NativeDestruct()
{
	Super::NativeDestruct();
}
