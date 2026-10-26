// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/PlayerHUDWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Animation/WidgetAnimation.h"
#include "Engine/Texture2D.h"
#include "Components/Overlay.h"

void UPlayerHUDWidget::SetHealth(float CurrentHealth, float MaxHealth)
{
	if (!HealthBar || MaxHealth <= 0.f)
	{
		return;
	}
	
	const float Percent = FMath::Clamp(CurrentHealth / MaxHealth, 0.f, 1.f);
	HealthBar->SetPercent(Percent);
	
	UpdateLowHealthEffect(Percent);
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

void UPlayerHUDWidget::SetCrosshairSpread(float Spread)
{
	if (!CrosshairTop || !CrosshairBottom || !CrosshairLeft || !CrosshairRight)
	{
		return;
	}
	
	const float Gap = FMath::Lerp(CrosshairMinGap, CrosshairMaxGap, FMath::Clamp(Spread, 0.f, 1.f));
	const float HalfBlade = CrosshairTop->GetBrush().ImageSize.Y*0.5f;
	const float Offset = Gap + HalfBlade;
	
	CrosshairTop->SetRenderTranslation(FVector2D(0.f, -Offset));
	CrosshairBottom->SetRenderTranslation(FVector2D(0.f, Offset));
	CrosshairLeft->SetRenderTranslation(FVector2D(-Offset, 0.f));
	CrosshairRight->SetRenderTranslation(FVector2D(Offset, 0.f));
}

void UPlayerHUDWidget::SetAiming(bool bIsAiming)
{
	if (!Crosshair)
	{
		return;
	}
	if (bIsAiming == true)
	{
		Crosshair->SetRenderOpacity(0);
	}
	else
	{
		Crosshair->SetRenderOpacity(1);
	}
}

void UPlayerHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetCrosshairSpread(0.f);
}

void UPlayerHUDWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

void UPlayerHUDWidget::UpdateLowHealthEffect(float HealthPercent)
{
	if (!BloodOverlay)
	{
		return;
	}
	
	// 체력이 충분하면 : 숨기고 박동 정지
	if (HealthPercent > LowHealthThreshold)
	{
		BloodOverlay->SetVisibility(ESlateVisibility::Collapsed);
		if (HeartbeatAnim && IsAnimationPlaying(HeartbeatAnim))
		{
			StopAnimation(HeartbeatAnim);
		}
		CurrentBloodStage = INDEX_NONE;
		return;
	}
	
	// 위험할 때 : 위험도 계산
	const float Danger = 1.f - (HealthPercent / LowHealthThreshold);
	
	if (BloodStageTextures.Num() > 0)
	{
		const int32 LastIndex = BloodStageTextures.Num() - 1;
		const int32 Stage = FMath::Clamp(FMath::FloorToInt(Danger * BloodStageTextures.Num()), 0, LastIndex);
		
		if (Stage != CurrentBloodStage && BloodStageTextures[Stage])
		{
			BloodOverlay->SetBrushFromTexture(BloodStageTextures[Stage]);
			
			CurrentBloodStage = Stage;
		}
	}
	
	BloodOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
	
	// 진하기 : 위험도 0이면 0.6, 위험도 1이면 1.0
	FLinearColor Color = BloodOverlay->GetColorAndOpacity();
	Color.A = FMath::Lerp(0.6f, 1.0f, Danger);
	BloodOverlay->SetColorAndOpacity(Color);
	
	// 박동 속도: 위험도 0이면 1배, 1이면 2.5배
	const float Speed = FMath::Lerp(1.f, 2.5f, Danger);
	
	if (HeartbeatAnim)
	{
		if (!IsAnimationPlaying(HeartbeatAnim))
		{
			PlayAnimation(HeartbeatAnim, 0.f, 0, EUMGSequencePlayMode::Forward, Speed);
		}
		else
		{
			SetPlaybackSpeed(HeartbeatAnim, Speed);
		}
	}

}
