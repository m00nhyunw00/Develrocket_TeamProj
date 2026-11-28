#include "AbilitySystem/Abilities/Player/TBGameplayAbility_Roll.h"

#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "TBGameplayTags.h"

UTBGameplayAbility_Roll::UTBGameplayAbility_Roll()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	
	SetAssetTags(FGameplayTagContainer(TBGameplayTags::Ability_Combat_Mobility));

	// 구르기 실행 중 일반 이동·점프 입력 차단
	ActivationOwnedTags.AddTag(TBGameplayTags::State_MovementBlocked);

	// 사망·기절 상태에서는 발동 불가
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Stunned);
}

void UTBGameplayAbility_Roll::OnActionCommitted()
{
	Super::OnActionCommitted();
	
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;

	if (!Movement || Movement->MovementMode == MOVE_None
		|| !FMath::IsFinite(RollDistance) || RollDistance <= 0.0f
		|| !FMath::IsFinite(RollDuration) || RollDuration <= 0.0f)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	
	// 구르기 방향
	FVector Direction = Character->GetActorForwardVector();
	Direction.Z = 0.0;
	Direction = Direction.GetSafeNormal();

	if (Direction.IsNearlyZero())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// 충돌 여부와 관계없이 같은 시간 동안 일정한 속도 적용
	const float RollSpeed = RollDistance / RollDuration;

	UAbilityTask_ApplyRootMotionConstantForce* Task =
		UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
			this, TEXT("Roll"), Direction, RollSpeed, RollDuration,
			false,
			nullptr,
			ERootMotionFinishVelocityMode::SetVelocity,
			FVector::ZeroVector,
			0.0f,
			true);
		

	if (!Task)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	
	Task->ReadyForActivation();
}