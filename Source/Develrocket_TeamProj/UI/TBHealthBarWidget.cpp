#include "UI/TBHealthBarWidget.h"
#include "Components/ProgressBar.h"

void UTBHealthBarWidget::SetHealth(float CurrentHealth, float MaxHealth)
{
	if (!HealthBar || MaxHealth <= 0.f)
	{
		return;
	}
	
	const float Percent = FMath::Clamp(CurrentHealth / MaxHealth, 0.f, 1.f);
	HealthBar->SetPercent(Percent);
}
