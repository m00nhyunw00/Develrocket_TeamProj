#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TBLeaderboardTypes.h"
#include "TBLeaderboardWidget.generated.h"

class UScrollBox;
class UTBLeaderboardRowWidget;

UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBLeaderboardWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	// 외부에서 받은 목록으로 화면의 행을 재구성
	UFUNCTION(BlueprintCallable, Category = "Leaderboard")
	void SetLeaderboardRows(const TArray<FTBLeaderboardRowData>& Rows);
	
	// 목록을 비우는 초기화 함수
	UFUNCTION(BlueprintCallable, Category = "Leaderboard")
	void ClearLeaderboardRows();
	
protected:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UScrollBox> PlayerRowsScrollBox;
	
	UPROPERTY(EditDefaultsOnly, Category = "Leaderboard")
	TSubclassOf<UTBLeaderboardRowWidget> RowWidgetClass;
};