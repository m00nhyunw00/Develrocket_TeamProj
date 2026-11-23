#include "UI/InGame/TBLeaderboardRowWidget.h"

#include "Components/TextBlock.h"

void UTBLeaderboardRowWidget::SetRowData(const FTBLeaderboardRowData& RowData)
{
	// 숫자 열을 순위와 통계 값으로 갱신
	if (RankText)
	{
		RankText->SetText(FText::AsNumber(RowData.Rank));
	}

	if (PlayerNameText)
	{
		PlayerNameText->SetText(RowData.PlayerName);
	}

	if (RoleText)
	{
		RoleText->SetText(RowData.ClassName);
	}

	if (KillsText)
	{
		KillsText->SetText(FText::AsNumber(RowData.Kills));
	}

	if (DeathsText)
	{
		DeathsText->SetText(FText::AsNumber(RowData.Deaths));
	}

	if (ScoreText)
	{
		ScoreText->SetText(FText::AsNumber(RowData.Score));
	}

	// 핑 값이 없는 경우 숫자 대신 대시를 표시
	if (PingText)
	{
		const FString PingString = RowData.PingMs >= 0
			? FString::Printf(TEXT("%d ms"), RowData.PingMs)
			: TEXT("-");

		PingText->SetText(FText::FromString(PingString));
	}
}
