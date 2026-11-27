#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TBClassSelectionSubsystem.generated.h"

class APlayerState;
class UTBCharacterClassData;
struct FUniqueNetIdRepl;

// 로비에서 고른 직업 보관 (서버 전용)
// 맵 이동 시 PlayerState는 새로 생성되므로 GameInstance에 보관 후 전장에서 복원
UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBClassSelectionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	void Clear() { Selections.Reset(); }

	// 플레이어 고유 ID 기준으로 직업 저장
	void SaveSelection(const APlayerState* PlayerState, const UTBCharacterClassData* ClassData);

	// 저장된 직업 조회 (없으면 nullptr)
	const UTBCharacterClassData* FindSelection(const FUniqueNetIdRepl& UniqueId) const;

private:
	// 키: 고유 ID 문자열
	// UPROPERTY로 맵 이동 중 데이터 에셋 GC 방지
	UPROPERTY()
	TMap<FString, TObjectPtr<const UTBCharacterClassData>> Selections;
};
