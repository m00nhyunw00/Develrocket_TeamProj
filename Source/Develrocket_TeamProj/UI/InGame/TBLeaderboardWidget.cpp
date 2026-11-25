#include "UI/InGame/TBLeaderboardWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "UI/InGame/TBLeaderboardRowWidget.h"

void UTBLeaderboardWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Canvas;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(FLinearColor(0.025f, 0.018f, 0.025f, 0.94f));
	Panel->SetPadding(FMargin(24.0f));
	UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.22f, 0.16f, 0.78f, 0.76f));
	PanelSlot->SetOffsets(FMargin(0.0f));

	PlayerRowsScrollBox = WidgetTree->ConstructWidget<UScrollBox>();
	Panel->SetContent(PlayerRowsScrollBox);
}

UHorizontalBox* UTBLeaderboardWidget::CreateDefaultRow(const TArray<FText>& Columns, bool bHighlight)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	for (int32 Index = 0; Index < Columns.Num(); ++Index)
	{
		UTextBlock* Cell = WidgetTree->ConstructWidget<UTextBlock>();
		Cell->SetText(Columns[Index]);
		Cell->SetClipping(EWidgetClipping::ClipToBounds);
		Cell->SetColorAndOpacity(FSlateColor(bHighlight ? FLinearColor(1.0f, 0.76f, 0.46f) : FLinearColor::White));

		FSlateChildSize Size(ESlateSizeRule::Fill);
		Size.Value = Index == 1 ? 3.0f : 1.0f;
		UHorizontalBoxSlot* CellSlot = Row->AddChildToHorizontalBox(Cell);
		CellSlot->SetSize(Size);
		CellSlot->SetPadding(FMargin(4.0f, 6.0f));
	}
	return Row;
}

void UTBLeaderboardWidget::SetLeaderboardRows(const TArray<FTBLeaderboardRowData>& Rows)
{
	// 새 목록을 넣기 전에 기존 행을 제거
	ClearLeaderboardRows();
	
	if (!PlayerRowsScrollBox)
	{
		return;
	}

	if (!RowWidgetClass)
	{
		PlayerRowsScrollBox->AddChild(CreateDefaultRow({
			NSLOCTEXT("TBLeaderboard", "Rank", "Rank"), NSLOCTEXT("TBLeaderboard", "Player", "Player"),
			NSLOCTEXT("TBLeaderboard", "Kills", "Kills"), NSLOCTEXT("TBLeaderboard", "Deaths", "Deaths"),
			NSLOCTEXT("TBLeaderboard", "Score", "Score")}, true));

		for (const FTBLeaderboardRowData& Row : Rows)
		{
			PlayerRowsScrollBox->AddChild(CreateDefaultRow({
				FText::AsNumber(Row.Rank), Row.PlayerName, FText::AsNumber(Row.Kills),
				FText::AsNumber(Row.Deaths), FText::AsNumber(Row.Score)}, Row.bIsLocalPlayer));
		}
		return;
	}
	
	// 전달된 데이터마다 행 위젯을 만들어 목록에 추가
	for (const FTBLeaderboardRowData& RowData : Rows)
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
