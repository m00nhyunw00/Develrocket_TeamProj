// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/PlayerHUDWidget.h"
#include "UI/HealthBarWidget.h"
#include "Components/Image.h"
#include "Animation/WidgetAnimation.h"
#include "Engine/Texture2D.h"
#include "UI/AmmoWidget.h"
#include "Components/Overlay.h"

void UPlayerHUDWidget::SetHealth(float CurrentHealth, float MaxHealth)
{
	if (HealthBarWidget)
	{
		HealthBarWidget->SetHealth(CurrentHealth, MaxHealth);
	}
	
	if (MaxHealth <= 0.f)
	{
		return;
	}
	const float Percent = FMath::Clamp(CurrentHealth / MaxHealth, 0.f, 1.f);
	UpdateLowHealthEffect(Percent);
}

void UPlayerHUDWidget::SetCrosshairSpread(float Spread)
{
	TargetSpread = FMath::Clamp(Spread, 0.f, 1.f);
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

void UPlayerHUDWidget::SetShowHitMarker(bool bIsKill)
{
	if (!HitMarker || !HitMarkerAnim)
	{
		return;
	}
	
	// 처치면 KillColor, 아니면 HitColor
	HitMarker->SetColorAndOpacity(bIsKill ? KillColor : HitColor);
	
	// 재생 중이어도 처음부터 다시
	PlayAnimation(HitMarkerAnim, 0.f, 1);
}

void UPlayerHUDWidget::SetAmmo(int32 CurrentAmmo, int32 ReserveAmmo, int32 MagazineSize)
{
	if (!AmmoWidget)
	{
		return;
	}
	
	AmmoWidget->SetAmmo(CurrentAmmo, ReserveAmmo, MagazineSize);
}

void UPlayerHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyCrosshairSpread(CurrentSpread);
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

void UPlayerHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	// 이미 목표에 거의 도착했으면 아무것도 안 함(매 프레임 배치하면 낭비)
	if (FMath::IsNearlyEqual(TargetSpread, CurrentSpread, 0.001f))
	{
		return;
	}
	
	CurrentSpread = FMath::FInterpTo(CurrentSpread, TargetSpread, InDeltaTime, CrosshairInterpSpeed);
	ApplyCrosshairSpread(CurrentSpread);
}

void UPlayerHUDWidget::ApplyCrosshairSpread(float Spread)
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
