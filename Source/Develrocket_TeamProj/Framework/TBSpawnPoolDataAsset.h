#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Framework/TBSpawnPoolData.h"
#include "TBSpawnPoolDataAsset.generated.h"

// 맵에서 사용할 액터별 스폰 설정을 하나의 데이터 에셋으로 묶습니다.
UCLASS(BlueprintType)
class DEVELROCKET_TEAMPROJ_API UTBSpawnPoolDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	// 각 항목은 액터 클래스 하나의 스폰 수량, 풀 크기, 주기 등을 정의합니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn")
	TArray<FTBSpawnPoolConfig> SpawnEntries;
};
