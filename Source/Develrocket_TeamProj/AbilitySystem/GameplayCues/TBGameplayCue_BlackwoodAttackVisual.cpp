#include "AbilitySystem/GameplayCues/TBGameplayCue_BlackwoodAttackVisual.h"

#include "NiagaraSystem.h"

bool UTBGameplayCue_BlackwoodAttackVisual::ResolveSettings(const FGameplayCueParameters& Parameters, FTBAttackVisualSettings& OutSettings) const
{
	const FTBBlackwoodAttackVisualSettings* Settings = BlackwoodAttackVisuals.FindByPredicate(
		[&Parameters](const FTBBlackwoodAttackVisualSettings& Candidate)
		{
			return Candidate.CueTag.IsValid() && Candidate.CueTag == Parameters.OriginalTag;
		});
	if (!Settings)
	{
		return false;
	}

	// 저장된 설정을 변경하지 않고 이번 실행에 사용할 일반 슬롯만 선택
	OutSettings = static_cast<const FTBAttackVisualSettings&>(*Settings);
	if (Parameters.NormalizedMagnitude > 0.5f)
	{
		OutSettings.NormalSystem = Settings->EnhancedSystem;
		if (IsValid(Settings->EnhancedTrailSystem.Get()))
		{
			OutSettings.TrailSystem = Settings->EnhancedTrailSystem;
		}
		if (IsValid(Settings->EnhancedMuzzleImpactSystem.Get()))
		{
			OutSettings.MuzzleImpactSystem = Settings->EnhancedMuzzleImpactSystem;
		}
	}

	return true;
}
