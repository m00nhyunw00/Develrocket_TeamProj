#include "UI/TBLowHealthOverlayWidget.h"
#include "Animation/WidgetAnimation.h"
#include "Engine/Texture2D.h"
#include "Components/Image.h"

void UTBLowHealthOverlayWidget::SetHealth(float CurrentHealth, float MaxHealth)
{
	if (MaxHealth <= 0.f)
	{
		return;
	}
	const float Percent = FMath::Clamp(CurrentHealth / MaxHealth, 0.f, 1.f);
	UpdateLowHealthEffect(Percent);
}

void UTBLowHealthOverlayWidget::UpdateLowHealthEffect(float HealthPercent)
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
