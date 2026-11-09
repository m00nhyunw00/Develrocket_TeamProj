#include "TBGameplayAbility_Monster_Stagger.h"

#include "TBGameplayTags.h"
#include "Character/Monster/TBMonster.h"

UTBGameplayAbility_Monster_Stagger::UTBGameplayAbility_Monster_Stagger()
{
	// 몬스터 그로기는 서버에서 실행
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	// 어빌리티가 유지되는 동안 그로기 상태 부여
	ActivationOwnedTags.AddTag(TBGameplayTags::State_Stunned);

	// 사망 중이거나 이미 그로기 상태일 때 활성화 방지
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Stunned);
}

void UTBGameplayAbility_Monster_Stagger::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const ATBMonster* Monster = ActorInfo ? Cast<ATBMonster>(ActorInfo->AvatarActor.Get()) : nullptr;

	// 몬스터 아바타와 서버 권한 검증
	if (!IsValid(Monster) || !ActorInfo->IsNetAuthority())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 공통 액션에서 비용 처리와 몽타주 재생 및 종료 처리
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}
