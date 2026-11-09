#include "AbilitySystem/Abilities/TBGameplayAbility_PrimaryAttack.h"

#include "AbilitySystem/TBAbilitySystemComponent.h"
#include "Character/Player/TBPlayer.h"
#include "TBGameplayTags.h"

UTBGameplayAbility_PrimaryAttack::UTBGameplayAbility_PrimaryAttack()
{
	SetAssetTags(FGameplayTagContainer(TBGameplayTags::Ability_Combat_PrimaryAttack));

	// 소유 클라이언트 예측 실행 및 서버 검증
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// 공격 시작 시 비용 적용
	bCommitOnActivate = true;

	// 일반 공격의 상태 및 실행 제한
	ActivationOwnedTags.AddTag(TBGameplayTags::State_Combat);

	ActivationBlockedTags.AddTag(TBGameplayTags::State_Combat);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Stunned);
}

void UTBGameplayAbility_PrimaryAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	ATBPlayer* Player =
		ActorInfo ? Cast<ATBPlayer>(ActorInfo->AvatarActor.Get()) : nullptr;

	// 플레이어 일반 공격의 필수 대상 및 비용 설정 검증
	if (!IsValid(Player) || !GetCostGameplayEffect())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 비용 처리 및 몽타주 재생
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!IsActive())
	{
		return;
	}

	// 서버 적중 판정 및 피해 적용
	if (ActorInfo->IsNetAuthority())
	{
		Player->PerformPrimaryAttackTrace();
	}

	// 소유 플레이어 화면의 발사 알림
	if (Player->IsLocallyControlled())
	{
		if (UTBAbilitySystemComponent* TBASC = Cast<UTBAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()))
		{
			TBASC->OnPrimaryShotFired.Broadcast();
		}
	}

	// 현재 단계에서는 부모의 몽타주 완료 처리에서 종료
}