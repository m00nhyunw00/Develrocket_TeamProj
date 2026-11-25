#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateColor.h"
#include "TBLeaderboardTypes.h"
#include "TBLeaderboardRowWidget.generated.h"

class UTextBlock;
class UImage;

// 플레이어 한 명의 정보를 표시하는 기본 행 위젯
UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBLeaderboardRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 전달받은 데이터로 각 열의 텍스트와 스타일을 갱신
	UFUNCTION(BlueprintCallable, Category = "Leaderboard")
	void SetRowData(const FTBLeaderboardRowData& RowData);

protected:
	// 텍스트 열 ----------------------

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> RankText;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> PlayerNameText;

	// 역할 열을 생략할 수 있도록 바인딩
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> RoleText;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> KillsText;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> DeathsText;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> ScoreText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> AssistText;

	// 디자인에서 핑 열을 뺄 수 있도록 Optional
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> PingText;

	// 이미지 ----------------------

	// 행 배경 (GDF 버튼 바)
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> RowBackground;

	// 초상 메달리온 틀 (GDF 스킬 노드)
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> PortraitFrame;

	// 1위 표식 (GDF_ORN_SMALL_A)
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> TopRankMark;

	// 사망 표시 문구 (데이터 단계에서 사용)
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> DeadStateText;

	// 스타일 (BP Class Defaults에서 지정) ----------------------

	UPROPERTY(EditDefaultsOnly, Category = "Leaderboard|Style")
	FSlateBrush NormalRowBrush;

	UPROPERTY(EditDefaultsOnly, Category = "Leaderboard|Style")
	FSlateBrush LocalRowBrush;

	UPROPERTY(EditDefaultsOnly, Category = "Leaderboard|Style")
	FSlateBrush DeadRowBrush;

	UPROPERTY(EditDefaultsOnly, Category = "Leaderboard|Style")
	FSlateBrush NormalPortraitBrush;

	UPROPERTY(EditDefaultsOnly, Category = "Leaderboard|Style")
	FSlateBrush LocalPortraitBrush;

	UPROPERTY(EditDefaultsOnly, Category = "Leaderboard|Style")
	FSlateColor DefaultScoreColor = FSlateColor(FLinearColor(0.82f, 0.69f, 0.48f));

	UPROPERTY(EditDefaultsOnly, Category = "Leaderboard|Style")
	FSlateColor NegativeScoreColor = FSlateColor(FLinearColor(0.88f, 0.35f, 0.37f));

private:
	// 순위를 로마 숫자로 변환 (범위를 벗어나면 아라비아 숫자)
	static FText MakeRankText(int32 Rank);

	// 사망 / 내 행 / 일반 상태에 맞춰 배경, 초상, 표식 적용
	void ApplyRowStyle(const FTBLeaderboardRowData& RowData);
};