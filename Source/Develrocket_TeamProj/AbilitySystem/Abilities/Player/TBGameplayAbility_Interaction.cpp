#include "AbilitySystem/Abilities/Player/TBGameplayAbility_Interaction.h"

#include "AbilitySystemComponent.h"
#include "TBGameplayTags.h"
#include "Character/Player/TBPlayer.h"
#include "Components/TBInteractionComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Interface/TBInteractInterface.h"
#include "Character/Player/TBPlayer.h"

class ATBPlayer;

UTBGameplayAbility_Interaction::UTBGameplayAbility_Interaction()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	
	SetAssetTags(FGameplayTagContainer(TBGameplayTags::Ability_Interaction));
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Stunned);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Draining);
	
	// InstancePerActor라 인스턴스가 재사용됨 -> 매번 초기화 시켜주어야한다.
	bInteractionBegun = false;
	bInteractionResolved = false;
}

void UTBGameplayAbility_Interaction::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    // 재사용되는 어빌리티 인스턴스의 실행 상태 초기화
    bInteractionBegun = false;
    bInteractionResolved = false;

    // 서버 판정 또는 소유 클라이언트의 상호작용 대상 조회
    AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
    UTBInteractionComponent* InteractionComp = Avatar ? Avatar->FindComponentByClass<UTBInteractionComponent>() : nullptr;

    AActor* Target = nullptr;

    if (InteractionComp)
    {
        Target = HasAuthority(&ActivationInfo)
            ? InteractionComp->FindTargetForServer()
            : InteractionComp->GetFocusedActor();
    }

    CurrentTarget = Target;

    // 상호작용 대상과 인터페이스 구현 여부 검증
    if (!IsValid(Target) || !Target->GetClass()->ImplementsInterface(UTBInteractInterface::StaticClass()))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 흡혈 상호작용 여부와 수행 플레이어 확인
    const bool bIsDrainInteraction = ITBInteractInterface::Execute_IsDrainInteraction(Target);
    ATBPlayer* Player = Cast<ATBPlayer>(Avatar);

    if (bIsDrainInteraction)
    {
        const UCharacterMovementComponent* MovementComponent = IsValid(Player) ? Player->GetCharacterMovement() : nullptr;

        // 점프 요청 또는 지상 이외의 상태에서 흡혈 시작 차단
        if (!IsValid(Player)
            || !IsValid(MovementComponent)
            || Player->bPressedJump
            || !MovementComponent->IsMovingOnGround())
        {
            EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
            return;
        }
    }

    // 대상의 3인칭·1인칭 상호작용 몽타주 조회
    ThirdPersonMontage = ITBInteractInterface::Execute_GetInteractMontage(Target);
    FirstPersonMontage = ITBInteractInterface::Execute_GetInteractFirstPersonMontage(Target);

    if (bIsDrainInteraction)
    {
        // 흡혈은 두 시점의 연출이 모두 필요한 상호작용
        if (!ThirdPersonMontage || !FirstPersonMontage)
        {
            EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
            return;
        }

        // 흡혈 상태 적용 실패 시 종료
        if (!ApplyDrainState(Handle, ActorInfo, ActivationInfo))
        {
            EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
            return;
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
    }

    // 부모의 검증·비용 처리·몽타주 재생
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // 부모 실행 중 어빌리티가 종료된 경우 중단
    if (!IsActive())
    {
        return;
    }

    // 몽타주 없는 대화 등은 공통 검증·비용 처리 이후 즉시 결과 전달 및 종료
    if (!ThirdPersonMontage && !FirstPersonMontage)
    {
        if (HasAuthority(&ActivationInfo))
        {
            ResolveInteraction();
        }

        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // 서버에서 시작 시 결과 처리 또는 상호작용 시작 알림
    if (HasAuthority(&ActivationInfo))
    {
        if (ITBInteractInterface::Execute_ShouldResolveOnStart(Target))
        {
            ResolveInteraction();
        }
        else
        {
            bInteractionBegun = true;
            ITBInteractInterface::Execute_BeginInteract(Target, Avatar);
        }
    }
}

void UTBGameplayAbility_Interaction::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (!IsActive())
	{
		return;
	}

	// 결과 처리 전에 종료된 상호작용의 취소 알림 여부 저장
	const bool bNotifyCancel = ActorInfo && ActorInfo->IsNetAuthority() && bInteractionBegun && !bInteractionResolved;

	bInteractionBegun = false;

	if (bNotifyCancel)
	{
		if (AActor* Target = CurrentTarget.Get())
		{
			ITBInteractInterface::Execute_CancelInteract(Target, ActorInfo->AvatarActor.Get());
		}
	}

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
	CurrentTarget.Reset();
	bInteractionResolved = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UTBGameplayAbility_Interaction::HandleActionMontageCompleted()
{
    AActor* Target = CurrentTarget.Get();

    // 흡혈: 몽타주 완료 시 결과 (서버만)
	if (CurrentActorInfo && CurrentActorInfo->IsNetAuthority() && Target && !ITBInteractInterface::Execute_ShouldResolveOnStart(Target))
    {
        ResolveInteraction();
    }

    Super::HandleActionMontageCompleted();
}

bool UTBGameplayAbility_Interaction::ApplyDrainState(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
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

void UTBGameplayAbility_Interaction::ResolveInteraction()
{
	// 서버에서 상호작용 결과를 한 번만 전달
	if (!CurrentActorInfo || !CurrentActorInfo->IsNetAuthority() || bInteractionResolved)
	{
		return;
	}

	AActor* Target = CurrentTarget.Get();
	AActor* Avatar = GetAvatarActorFromActorInfo();
	UTBInteractionComponent* InteractionComp = Avatar ? Avatar->FindComponentByClass<UTBInteractionComponent>() : nullptr;

	if (!IsValid(Target) || !IsValid(InteractionComp))
	{
		return;
	}

	// 결과 호출 여부 기록
	bInteractionResolved = true;

	// 서버에서 상호작용 결과 처리
	ITBInteractInterface::Execute_Interact(Target, Avatar);

	// 소유 플레이어에게 로컬 결과 전달
	if (IsValid(Target) && IsValid(InteractionComp))
	{
		InteractionComp->Client_NotifyInteracted(Target);
	}
}
