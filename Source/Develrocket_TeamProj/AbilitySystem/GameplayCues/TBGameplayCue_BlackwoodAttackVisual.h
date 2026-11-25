#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/GameplayCues/TBGameplayCue_AttackVisual.h"
#include "TBGameplayCue_BlackwoodAttackVisual.generated.h"

// 공통 공격 연출에 추가할 블랙우드 강화 설정
USTRUCT(BlueprintType)
struct DEVELROCKET_TEAMPROJ_API FTBBlackwoodAttackVisualSettings : public FTBAttackVisualSettings
{
	GENERATED_BODY()

	// 강화 공격의 부착 이펙트, 미지정 시 재생 생략
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackwood|Enhanced")
	TObjectPtr<UNiagaraSystem> EnhancedSystem = nullptr;

	// 강화 공격의 총격 궤적, 미지정 시 일반 궤적 사용
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackwood|Enhanced")
	TObjectPtr<UNiagaraSystem> EnhancedTrailSystem = nullptr;

	// 강화 공격의 총구 발사 이펙트, 미지정 시 일반 이펙트 사용
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blackwood|Enhanced")
	TObjectPtr<UNiagaraSystem> EnhancedMuzzleImpactSystem = nullptr;
};

UCLASS(Blueprintable, HideCategories = (AttackVisuals))
class DEVELROCKET_TEAMPROJ_API UTBGameplayCue_BlackwoodAttackVisual : public UTBGameplayCue_AttackVisual
{
	GENERATED_BODY()

protected:
	// GameplayCue.Blackwood.Attack 하위 태그별 일반·강화 연출
	UPROPERTY(EditDefaultsOnly, Category = "BlackwoodAttackVisuals", meta = (TitleProperty = "CueTag"))
	TArray<FTBBlackwoodAttackVisualSettings> BlackwoodAttackVisuals;

	// 발사 시점의 강화 판정을 반영한 공통 연출 설정 선택
	virtual bool ResolveSettings(const FGameplayCueParameters& Parameters, FTBAttackVisualSettings& OutSettings) const override;
};
