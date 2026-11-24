#pragma once

#include "CoreMinimal.h"
#include "TBLeaderboardTypes.generated.h"

// 리더보드 한 행에 표시할 값
USTRUCT(BlueprintType)
struct DEVELROCKET_TEAMPROJ_API FTBLeaderboardRowData
{
	GENERATED_BODY()
	
	// 표시 순위
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Leaderboard")
	int32 Rank = 0;
	
	// 플레이어 이름
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Leaderboard")
	FText PlayerName;
	
	// 캐릭터 클래스 이름
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Leaderboard")
	FText ClassName;
	
	// 처치 수 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Leaderboard")
	int32 Kills = 0;
	
	// 사망 수
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Leaderboard")
	int32 Deaths = 0;
	
	// 점수 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Leaderboard")
	int32 Score = 0;
	
	// 도움 수 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Leaderboard")
	int32 Assists = 0;
	
	// 핑이 아직 없을 때는 INDEX_NONE을 사용
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Leaderboard")
	int32 PingMs = INDEX_NONE;
};
