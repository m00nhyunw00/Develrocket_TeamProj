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

void UTBGameplayAbility_Interaction::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// 재사용되는 어빌리티 인스턴스의 실행 상태 초기화
	bInteractionBegun = false;
	bInteractionResolved = false;
	
	// 1. 대상 찾기(서버는 판정용, 클라이언트는 보여주기 용)
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UTBInteractionComponent* InteractionComp = Avatar ? Avatar->FindComponentByClass<UTBInteractionComponent>(): nullptr;
	
	AActor* Target = nullptr;
	if (InteractionComp)
	{
		Target = HasAuthority(&ActivationInfo) ? InteractionComp->FindTargetForServer() : InteractionComp->GetFocusedActor();
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
		const UCharacterMovementComponent* MovementComponent =
			IsValid(Player) ? Player->GetCharacterMovement() : nullptr;

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

	// 기존 대상 몽타주 조회
	ThirdPersonMontage = Target ? ITBInteractInterface::Execute_GetInteractMontage(Target) : nullptr;
	FirstPersonMontage = Target ? ITBInteractInterface::Execute_GetInteractFirstPersonMontage(Target) : nullptr;
	
	// 2. 몽타주 채우기 (대상이 없으면 반드시 비우기)
	ThirdPersonMontage = Target ? ITBInteractInterface::Execute_GetInteractMontage(Target) : nullptr;
	FirstPersonMontage = Target ? ITBInteractInterface::Execute_GetInteractFirstPersonMontage(Target) : nullptr;
	
	if (!ThirdPersonMontage)
	{
		if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
		
		if (HasAuthority(&ActivationInfo) && Target)
		{
			ResolveInteraction();
		}
		
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}
	
	// 몽타주를 사용하는 흡혈 상호작용에만 상태 적용
	if (ITBInteractInterface::Execute_IsDrainInteraction(Target) && !ApplyDrainState(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	// 3. 부모: 검증 + Commit + 몽타주 재생	
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
	// 4. 부모가 종료시켰으면 중단
	if (!IsActive())
	{
		return;
	}
	
	// 4-1. 흡혈(결과가 끝에 나는 상호작용)이면 내 화면 카메라를 애니 커브로 전환
	if (IsLocallyControlled() && !ITBInteractInterface::Execute_ShouldResolveOnStart(Target))
	{
		if (ATBPlayer* Player = Cast<ATBPlayer>(GetAvatarActorFromActorInfo()))
		{
			Player->BeginFeedCamera();
		}
	}
	
	// 5. 서버: 시작 시 결과(대화) 또는 시작 알림(흡혈)
	if (HasAuthority(&ActivationInfo))
	{
		if (ITBInteractInterface::Execute_ShouldResolveOnStart(Target))
		{
			ResolveInteraction();
		}
		else
		{
			bInteractionBegun = true;
			ITBInteractInterface::Execute_BeginInteract(Target, GetAvatarActorFromActorInfo());
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
