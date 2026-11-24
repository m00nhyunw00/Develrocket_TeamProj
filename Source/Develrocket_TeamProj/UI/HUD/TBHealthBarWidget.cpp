#include "UI/HUD/TBHealthBarWidget.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ProgressBar.h"

void UTBHealthBarWidget::SetHealth(float CurrentHealth, float MaxHealth)
{
	// 최대 체력이 바뀔 때만 눈금 재생성 
	if (!FMath::IsNearlyEqual(MaxHealth,CachedMaxHealth))
	{
		CachedMaxHealth = MaxHealth;
		RebuildTicks(MaxHealth);
	}
	
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

	// 투명 여백을 제외한 빨간 영역 기준으로 표시값 보정
	const float DisplayPercent =
		FMath::Lerp(FillStart, FillEnd, HealthRatio);

	HealthBar->SetPercent(DisplayPercent);
}

void UTBHealthBarWidget::RebuildTicks(float MaxHealth)
{
	if (!TickLayer)
	{
		return;
	}
	
	TickLayer->ClearChildren();
	
	// 간격이 0 이하면 무한 루프가 되므로 반드시 막음
	if (!TickTexture || TickInterval <= 0.0f || MaxHealth <= 0.0f)
	{
		return;
	}
	
	// float 누적 오차를 피하려고 정수 인덱스로 반복
	for (int32 Index = 1; ; ++Index)
	{
		const float TickHealth = TickInterval * Index;
		
		// 끝(최대 체력) 위치에는 눈금 불필요
		if (TickHealth >= MaxHealth - KINDA_SMALL_NUMBER)
		{
			break;
		}

		UImage* Tick = WidgetTree->ConstructWidget<UImage>();
		Tick->SetBrushFromTexture(TickTexture);

		// 위치·크기는 Image가 아니라 부모에 붙을 때 생기는 Slot이 가진다
		UCanvasPanelSlot* TickSlot = TickLayer->AddChildToCanvas(Tick);
		if (!TickSlot)
		{
			continue;
		}

		// 채움과 같은 Lerp를 써야 체력 끝과 눈금이 정확히 겹침
		const float X = FMath::Lerp(FillStart, FillEnd, TickHealth / MaxHealth);

		// 앵커를 비율로 주고 Offset 0 → 바 크기가 바뀌어도 같이 늘고 줄어듦
		TickSlot->SetAnchors(FAnchors(X - TickHalfWidth, TickTop, X + TickHalfWidth, TickBottom));
		TickSlot->SetOffsets(FMargin(0.0f));
	}
}
