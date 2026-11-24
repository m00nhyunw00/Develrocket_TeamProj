#include "UI/InGame/TBLeaderboardWidget.h"

#include "Components/ScrollBox.h"
#include "UI/InGame/TBLeaderboardRowWidget.h"

void UTBLeaderboardWidget::SetLeaderboardRows(const TArray<FTBLeaderboardRowData>& Rows)
{
	// ScrollBox나 행 클래스가 연결되지 않았다면 중단
	if (!PlayerRowsScrollBox || !RowWidgetClass)
	{
		return;
	}

	// 모자란 행만 새로 만들어 보관 (이미 있는 행은 재사용)
	while (RowWidgetPool.Num() < Rows.Num())
	{
		UTBLeaderboardRowWidget* RowWidget = CreateWidget<UTBLeaderboardRowWidget>(this, RowWidgetClass);
		if (!RowWidget)
		{
			return;
		}

		PlayerRowsScrollBox->AddChild(RowWidget);
		RowWidgetPool.Add(RowWidget);
	}

	// 데이터가 있는 행은 갱신·표시, 남는 행은 숨김
	for (int32 Index = 0; Index < RowWidgetPool.Num(); ++Index)
	{
		UTBLeaderboardRowWidget* RowWidget = RowWidgetPool[Index];
		if (!RowWidget)
		{
			continue;
		}

		if (Rows.IsValidIndex(Index))
		{
			RowWidget->SetRowData(Rows[Index]);
			RowWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			RowWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UTBLeaderboardWidget::ClearLeaderboardRows()
{
	// 완전 초기화: 행 위젯과 보관 목록을 모두 비움
	if (PlayerRowsScrollBox)
	{
		PlayerRowsScrollBox->ClearChildren();
	}

	RowWidgetPool.Reset();
}
