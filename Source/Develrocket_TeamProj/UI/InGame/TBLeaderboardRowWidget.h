#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TBLeaderboardTypes.h"
#include "TBLeaderboardRowWidget.generated.h"

class UTextBlock;

// 플레이어 한 명의 정보를 표시하는 기본 행 위젯
UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBLeaderboardRowWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	// 전달받은 데이터로 각 열의 텍스트를 갱싱
	UFUNCTION(BlueprintCallable, Category = "Leaderboard")
	void SetRowData(const FTBLeaderboardRowData& RowData);
	
protected:
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
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> PingText;
};