#include "AbilitySystem/Abilities/Player/TBGameplayAbility_Instant.h"

#include "TBGameplayTags.h"

UTBGameplayAbility_Instant::UTBGameplayAbility_Instant()
{
	// 부모 생성자의 예측 실행, 차단 태그는 그대로 물려받음
	// 어빌리티 태그만 즉시 결과 절차용으로 교체
	SetAssetTags(FGameplayTagContainer(TBGameplayTags::Ability_Interaction_Instant));
}

bool UTBGameplayAbility_Instant::ShouldResolveOnStart() const
{
	// 즉시 결과: 몽타주를 기다리지 않음
	return true;
}