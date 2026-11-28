#include "AbilitySystem/Abilities/Player/TBGameplayAbility_SwordWave.h"

#include "TBGameplayTags.h"

UTBGameplayAbility_SwordWave::UTBGameplayAbility_SwordWave()
{
	// 아마나이 스킬 2의 입력 태그와 피해량 조회 기준
	SetAssetTags(FGameplayTagContainer(TBGameplayTags::Ability_Combat_Skill2));
	AttackId = TBGameplayTags::Ability_Combat_Skill2;

	// 몽타주 종료 또는 허용된 입력에 의한 취소까지 동작 유지
	bWaitForRecovery = true;
}

bool UTBGameplayAbility_SwordWave::GetProjectileLaunchTransform_Implementation(FTransform& OutTransform) const
{
	if (!FMath::IsFinite(ProjectileRoll) || !Super::GetProjectileLaunchTransform_Implementation(OutTransform))
	{
		return false;
	}

	// 발사 방향인 로컬 X축을 기준으로 초승달 회전
	const FQuat RollOffset(FVector::ForwardVector, FMath::DegreesToRadians(ProjectileRoll));

	OutTransform.SetRotation((OutTransform.GetRotation() * RollOffset).GetNormalized());

	return OutTransform.IsValid();
}