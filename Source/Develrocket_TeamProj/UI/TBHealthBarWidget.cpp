#include "UI/TBHealthBarWidget.h"
#include "Components/ProgressBar.h"

void UTBHealthBarWidget::SetHealth(float CurrentHealth, float MaxHealth)
{
	if (!HealthBar)
	{
		return;
	}

	// 실제 체력 비율
	const float HealthRatio = MaxHealth > 0.0f
		? FMath::Clamp(CurrentHealth / MaxHealth, 0.0f, 1.0f)
		: 0.0f;

	if (HealthRatio <= 0.0f)
	{
		HealthBar->SetPercent(0.0f);
		return;
	}

	// 현재 원본 이미지에서 빨간 영역이 차지하는 가로 구간
	constexpr float FillStart = 176.0f / 1600.0f;
	constexpr float FillEnd = 1425.0f / 1600.0f;

	// 투명 여백을 제외한 빨간 영역 기준으로 표시값 보정
	const float DisplayPercent =
		FMath::Lerp(FillStart, FillEnd, HealthRatio);

	HealthBar->SetPercent(DisplayPercent);
}
