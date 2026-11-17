#include "AbilitySystem/Abilities/Player/TBGameplayAbility_Interaction.h"

#include "AbilitySystemComponent.h"
#include "TBGameplayTags.h"
#include "Components/TBInteractionComponent.h"
#include "Interface/TBInteractInterface.h"

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
	
	// 대상이 요청한 절차가 이 어빌리티인지 검증
	// 서버는 직접 다시 찾은 대상에게 다시 묻기 때문에, 클라이언트가 고른 값을 믿지 않음
	const FGameplayTag RequestedTag = ITBInteractInterface::Execute_GetInteractionAbilityTag(Target, Avatar);

	if (!RequestedTag.IsValid() || !GetAssetTags().HasTagExact(RequestedTag))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

    // 대상의 3인칭·1인칭 상호작용 몽타주 조회
    ThirdPersonMontage = ITBInteractInterface::Execute_GetInteractMontage(Target);
    FirstPersonMontage = ITBInteractInterface::Execute_GetInteractFirstPersonMontage(Target);

   // 시작 조건
	if (!CanStartInteraction(ActorInfo, Target))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	// 준비
	if (!PrepareInteraction(Handle, ActorInfo, ActivationInfo, Target))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
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
        if (ShouldResolveOnStart())
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
	
	// 정리
	CleanupInteraction(ActorInfo, CurrentTarget.Get());

	// 다음 상호작용을 위한 실행 정보 정리
	CurrentTarget.Reset();
	bInteractionResolved = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UTBGameplayAbility_Interaction::HandleActionMontageCompleted()
{
    AActor* Target = CurrentTarget.Get();

	// 몽타주 완료 시 결과를 내는 절차 (서버만)
	if (CurrentActorInfo && CurrentActorInfo->IsNetAuthority() && Target && !ShouldResolveOnStart())
    {
        ResolveInteraction();
    }

    Super::HandleActionMontageCompleted();
}

bool UTBGameplayAbility_Interaction::CanStartInteraction(const FGameplayAbilityActorInfo* ActorInfo,
	AActor* Target) const
{
	return true;
}

bool UTBGameplayAbility_Interaction::PrepareInteraction(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, AActor* Target)
{
	return true;
}

void UTBGameplayAbility_Interaction::CleanupInteraction(const FGameplayAbilityActorInfo* ActorInfo, AActor* Target)
{
	
}

bool UTBGameplayAbility_Interaction::ShouldResolveOnStart() const
{
	return false;
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
