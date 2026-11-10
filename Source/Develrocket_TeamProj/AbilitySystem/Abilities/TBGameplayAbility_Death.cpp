#include "AbilitySystem/Abilities/TBGameplayAbility_Death.h"
#include "TBGameplayTags.h"

UTBGameplayAbility_Death::UTBGameplayAbility_Death()
{
	SetAssetTags(FGameplayTagContainer(TBGameplayTags::Ability_Death));

	// 사망 확정과 실행은 서버에서 처리
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnly;

	// 사망 연출에는 비용과 쿨다운 적용 생략
	bCommitOnActivate = false;

	// 사망 상태가 확정된 경우에만 실행
	ActivationRequiredTags.AddTag(TBGameplayTags::State_Dead);
}