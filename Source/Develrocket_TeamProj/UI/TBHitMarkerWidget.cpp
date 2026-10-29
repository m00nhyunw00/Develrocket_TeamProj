#include "UI/TBHitMarkerWidget.h"
#include "Components/Image.h"

void UTBHitMarkerWidget::SetShowHitMarker(bool bIsKill)
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
