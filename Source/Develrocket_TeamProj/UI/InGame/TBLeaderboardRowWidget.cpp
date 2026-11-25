#include "UI/InGame/TBLeaderboardRowWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"

void UTBLeaderboardRowWidget::SetRowData(const FTBLeaderboardRowData& RowData)
{
	// 순위는 로마 숫자로 표시
	if (RankText)
	{
		RankText->SetText(MakeRankText(RowData.Rank));
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

	// 음수 점수는 경고 색, 그 외는 기본 색 (행 재사용에 대비해 매번 둘 중 하나를 설정)
	if (ScoreText)
	{
		ScoreText->SetText(FText::AsNumber(RowData.Score));
		ScoreText->SetColorAndOpacity(RowData.Score < 0 ? NegativeScoreColor : DefaultScoreColor);
	}

	if (AssistText)
	{
		AssistText->SetText(FText::AsNumber(RowData.Assists));
	}

	// 핑 값이 없는 경우 숫자 대신 대시를 표시
	if (PingText)
	{
		const FString PingString = RowData.PingMs >= 0
			? FString::Printf(TEXT("%d ms"), RowData.PingMs)
			: TEXT("-");

		PingText->SetText(FText::FromString(PingString));
	}

	ApplyRowStyle(RowData);
}

FText UTBLeaderboardRowWidget::MakeRankText(int32 Rank)
{
	// 최대 인원 6명 기준, 여유분으로 8까지
	static const TCHAR* RomanNumerals[] =
	{
		TEXT("I"), TEXT("II"), TEXT("III"), TEXT("IV"),
		TEXT("V"), TEXT("VI"), TEXT("VII"), TEXT("VIII")
	};

	if (Rank >= 1 && Rank <= UE_ARRAY_COUNT(RomanNumerals))
	{
		return FText::FromString(RomanNumerals[Rank - 1]);
	}

	return FText::AsNumber(Rank);
}

void UTBLeaderboardRowWidget::ApplyRowStyle(const FTBLeaderboardRowData& RowData)
{
	// 행 배경: 일반 → 내 행 → 사망 순으로 덮어써서 사망이 최우선
	if (RowBackground)
	{
		const FSlateBrush* RowBrush = &NormalRowBrush;

		if (RowData.bIsLocalPlayer)
		{
			RowBrush = &LocalRowBrush;
		}

		if (RowData.bIsDead)
		{
			RowBrush = &DeadRowBrush;
		}

		RowBackground->SetBrush(*RowBrush);
	}

	// 초상 틀: 내 행만 붉은 테
	if (PortraitFrame)
	{
		PortraitFrame->SetBrush(RowData.bIsLocalPlayer ? LocalPortraitBrush : NormalPortraitBrush);
	}

	// 1위에만 표식 표시
	if (TopRankMark)
	{
		TopRankMark->SetVisibility(RowData.Rank == 1
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}

	// 사망 상태일 때만 문구 표시
	if (DeadStateText)
	{
		DeadStateText->SetVisibility(RowData.bIsDead
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}
}