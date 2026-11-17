#include "AbilitySystem/Abilities/Player/TBGameplayAbility_Drain.h"

#include "AbilitySystemComponent.h"
#include "TBGameplayTags.h"
#include "Character/Player/TBPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

UTBGameplayAbility_Drain::UTBGameplayAbility_Drain()
{
	SetAssetTags(FGameplayTagContainer(TBGameplayTags::Ability_Interaction_Drain));
	ActivationOwnedTags.AddTag(TBGameplayTags::State_Draining);
	ActivationOwnedTags.AddTag(TBGameplayTags::State_MovementBlocked);
}

bool UTBGameplayAbility_Drain::CanStartInteraction(const FGameplayAbilityActorInfo* ActorInfo, AActor* Target) const
{
	if (!Super::CanStartInteraction(ActorInfo, Target))
	{
		return false;
	}
	
	const ATBPlayer* Player = ActorInfo ? Cast<ATBPlayer>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UCharacterMovementComponent* MovementComponent = IsValid(Player) ? Player->GetCharacterMovement() : nullptr;

	// 점프 요청 또는 지상 이외의 상태에서 흡혈 시작 차단
	return IsValid(Player)
		&& IsValid(MovementComponent)
		&& !Player->bPressedJump
		&& MovementComponent->IsMovingOnGround();
}

bool UTBGameplayAbility_Drain::PrepareInteraction(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, AActor* Target)
{
	if (!Super::PrepareInteraction(Handle, ActorInfo, ActivationInfo, Target))
	{
		return false;
	}

	// 흡혈은 두 시점의 연출이 모두 필요한 상호작용
	if (!ThirdPersonMontage || !FirstPersonMontage)
	{
		return false;
	}

	// 전투 취소·입력 정리 실패 시 중단
	if (!PrepareDrainInput(Handle, ActorInfo, ActivationInfo))
	{
		return false;
	}

	// 플레이어 방향 정렬
	ATBPlayer* Player = Cast<ATBPlayer>(ActorInfo->AvatarActor.Get());
	if (!IsValid(Player))
	{
		return false;
	}

	// 플레이어에서 시민으로 향하는 수평 방향 계산
	FVector ToTarget = Target->GetActorLocation() - Player->GetActorLocation();
	ToTarget.Z = 0.0f;

	// 수평 위치가 겹치는 경우 기존 몸체 방향 유지
	const float TargetYaw = ToTarget.IsNearlyZero()
		? Player->GetActorRotation().Yaw
		: ToTarget.Rotation().Yaw;

	// 상하·좌우 기울기를 제거한 수평 정면 회전
	const FRotator FacingRotation(0.0f, TargetYaw, 0.0f);

	// 이후 시민 정렬에서 사용할 플레이어 몸체 방향 적용
	Player->SetActorRotation(FacingRotation);

	if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
	{
		// 현재 프레임에 누적된 회전 입력 정리
		PC->RotationInput = FRotator::ZeroRotator;

		// 플레이어 카메라 방향 정렬
		PC->SetControlRotation(FacingRotation);

		// 서버의 확정 방향을 소유 클라이언트에 전달
		if (ActorInfo->IsNetAuthority())
		{
			PC->ClientSetRotation(FacingRotation, true);
		}
	}

	return true;
}



bool UTBGameplayAbility_Drain::PrepareDrainInput(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!IsValid(ASC))
	{
		return false;
	}

	// 실행 중인 전투 어빌리티 취소 (지상 검사를 통과한 뒤에만 취소되도록 코드에서 처리)
	const FGameplayTagContainer CombatTags(TBGameplayTags::Ability_Combat);
	ASC->CancelAbilities(&CombatTags, nullptr, this);

	// 다른 어빌리티 취소 중 연쇄 반응으로 이 어빌리티가 끝났는지 확인
	if (!IsActive())
	{
		return false;
	}

	// 흡혈 시작 전 남아 있는 이동 입력 정리
	if (ATBPlayer* Player = Cast<ATBPlayer>(ActorInfo->AvatarActor.Get()))
	{
		Player->ResetInputForDraining();
	}

	return true;
}