#include "UI/InGame/TBLeaderboardWidget.h"

#include "Components/ScrollBox.h"
#include "UI/InGame/TBLeaderboardRowWidget.h"

void UTBLeaderboardWidget::SetLeaderboardRows(const TArray<FTBLeaderboardRowData>& Rows)
{
	// 새 목록을 넣기 전에 기존 행을 제거
	ClearLeaderboardRows();
	
	// ScrollBox나 행 클래스가 연결되지 않았다면 생성을 중단
	if (!PlayerRowsScrollBox || !RowWidgetClass)
	{
		return;
	}
	
	// 전달된 데이터마다 행 위젯을 만들어 목록에 추가
	for (const FTBLeaderboardRowData& RowData : Rows)
	{
		UTBLeaderboardRowWidget* RowWidget = CreateWidget<UTBLeaderboardRowWidget>(this, RowWidgetClass);
		
		if (!RowWidget)
		{
			continue;
		}
		
		// 행 데이터를 행 위젯의 TextBlock들에 적용
		RowWidget->SetRowData(RowData);
		PlayerRowsScrollBox->AddChild(RowWidget);
	}
}

void UTBLeaderboardWidget::ClearLeaderboardRows()
{
	// ScrollBox의 이전 리더보드 행을 모두 제거
	if (PlayerRowsScrollBox)
	{
		PlayerRowsScrollBox->ClearChildren();
	}
}
