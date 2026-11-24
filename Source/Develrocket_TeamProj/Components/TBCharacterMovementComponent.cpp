#include "TBCharacterMovementComponent.h"

#include "AbilitySystemComponent.h"
#include "TBGameplayTags.h"
#include "AbilitySystem/Attributes/TBPlayerAttributeSet.h"
#include "Character/TBCharacterBase.h"

bool UTBCharacterMovementComponent::IsMovementBlockedByState() const
{
	const ATBCharacterBase* Character = Cast<ATBCharacterBase>(CharacterOwner);

	return Character && Character->IsMovementBlocked();
}

float UTBCharacterMovementComponent::GetMaxSpeed() const
{
	// 사망·스턴 등의 기존 이동 차단 우선 적용
	if (IsMovementBlockedByState())
	{
		return 0.0f;
	}

	float MaxSpeed = Super::GetMaxSpeed();

	const ATBCharacterBase* Character = Cast<ATBCharacterBase>(CharacterOwner);

	const UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;

	if (!IsValid(ASC))
	{
		return MaxSpeed;
	}

	// 지상·낙하 이동에 플레이어의 GAS 속도 보너스 적용
	if (IsMovingOnGround() || IsFalling())
	{
		const UTBPlayerAttributeSet* PlayerAttributes = ASC->GetSet<UTBPlayerAttributeSet>();

		if (IsValid(PlayerAttributes))
		{
			const float Bonus = PlayerAttributes->GetMoveSpeedBonus();

			MaxSpeed *= 1.0f + FMath::Max(0.0f, Bonus);
		}
	}

	// 기존 조준 감속 유지
	if (ASC->HasMatchingGameplayTag(TBGameplayTags::State_Aiming))
	{
		MaxSpeed *= FMath::Clamp(AimMovementSpeedMultiplier, 0.0f, 1.0f);
	}

	return MaxSpeed;
}

float UTBCharacterMovementComponent::GetMaxAcceleration() const
{
	if (IsMovementBlockedByState())
	{
		return 0.0f;
	}

	return Super::GetMaxAcceleration();
}

bool UTBCharacterMovementComponent::CanAttemptJump() const
{
	return !IsMovementBlockedByState() && Super::CanAttemptJump();
}
