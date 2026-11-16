#include "AbilitySystem/Abilities/Player/TBGameplayAbility_Drain.h"

#include "AbilitySystemComponent.h"
#include "TBGameplayTags.h"
#include "Character/Player/TBPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"

bool UTBGameplayAbility_Drain::CanStartInteraction(const FGameplayAbilityActorInfo* ActorInfo, AActor* Target) const
{
	//  부모의 공통조건을 먼저 확인한다.
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

bool UTBGameplayAbility_Drain::PrepareInteraction(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, AActor* Target)
{
	if (!Super::PrepareInteraction(Handle, ActorInfo, ActivationInfo, Target))
	{
		return false;
	}
	
	// 흡혈 상태 적용 실패 시 중단
	if (!ApplyDrainState(Handle, ActorInfo, ActivationInfo))
	{
		return false;
	}

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

void UTBGameplayAbility_Drain::CleanupInteraction(const FGameplayAbilityActorInfo* ActorInfo)
{
	// 서버에서 이번 흡혈이 적용한 상태 GE만 제거
	if (ActorInfo && ActorInfo->IsNetAuthority() && DrainStateEffectHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = DrainStateASC.Get())
		{
			ASC->RemoveActiveGameplayEffect(DrainStateEffectHandle);
		}
	}

	// 다음 상호작용을 위한 실행 정보 정리
	DrainStateEffectHandle.Invalidate();
	DrainStateASC.Reset();
	
	Super::CleanupInteraction(ActorInfo);
}

bool UTBGameplayAbility_Drain::ApplyDrainState(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const UGameplayEffect* Effect = DrainStateEffectClass ? DrainStateEffectClass.GetDefaultObject() : nullptr;

	// 무한 지속시간과 필수 부여 태그 검증
	if (!IsValid(ASC)
		|| !Effect
		|| Effect->DurationPolicy != EGameplayEffectDurationType::Infinite
		|| !Effect->GetGrantedTags().HasTagExact(TBGameplayTags::State_Draining)
		|| !Effect->GetGrantedTags().HasTagExact(TBGameplayTags::State_MovementBlocked))
	{
		return false;
	}

	const FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo, DrainStateEffectClass, 1.0f);

	if (!Spec.IsValid())
	{
		return false;
	}

	// 소유 클라이언트 예측 및 서버 상태 적용
	DrainStateEffectHandle = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);

	if (!DrainStateEffectHandle.IsValid())
	{
		return false;
	}

	DrainStateASC = ASC;

	// 실행 중인 전투 어빌리티 취소
	const FGameplayTagContainer CombatTags(TBGameplayTags::Ability_Combat);
	ASC->CancelAbilities(&CombatTags, nullptr, this);

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


