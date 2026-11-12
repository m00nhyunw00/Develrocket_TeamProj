#include "AbilitySystem/Abilities/Player/TBGameplayAbility_Interaction.h"

#include "TBGameplayTags.h"
#include "Components/TBInteractionComponent.h"
#include "Interface/TBInteractable.h"

UTBGameplayAbility_Interaction::UTBGameplayAbility_Interaction()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	
	SetAssetTags(FGameplayTagContainer(TBGameplayTags::Ability_Interaction));
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Dead);
	
	// InstancePerActor라 인스턴스가 재사용됨 -> 매번 초기화 시켜주어야한다.
	bInteractionBegun = false;
	bInteractionResolved = false;
}

void UTBGameplayAbility_Interaction::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// 1. 대상 찾기(서버는 판정용, 클라이언트는 보여주기 용)
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UTBInteractionComponent* InteractionComp = Avatar ? Avatar->FindComponentByClass<UTBInteractionComponent>(): nullptr;
	
	AActor* Target = nullptr;
	if (InteractionComp)
	{
		Target = HasAuthority(&ActivationInfo) ? InteractionComp->FindTargetForServer() : InteractionComp->GetFocusedActor();
	}
	CurrentTarget = Target;
	
	// 2. 몽타주 채우기 (대상이 없으면 반드시 비우기)
	ThirdPersonMontage = Target ? ITBInteractable::Execute_GetInteractMontage(Target) : nullptr;
	FirstPersonMontage = Target ? ITBInteractable::Execute_GetInteractFirstPersonMontage(Target) : nullptr;
	
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
	
	// 3. 부모: 검증 + Commit + 몽타주 재생	
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
	// 4. 부모가 종료시켰으면 중단
	if (!IsActive())
	{
		return;
	}
	
	// 5. 서버: 시작 시 결과(대화) 또는 시작 알림(흡혈)
	if (HasAuthority(&ActivationInfo))
	{
		if (ITBInteractable::Execute_ShouldResolveOnStart(Target))
		{
			ResolveInteraction();
		}
		else
		{
			bInteractionBegun = true;
			ITBInteractable::Execute_BeginInteract(Target, GetAvatarActorFromActorInfo());
		}
	}
}

void UTBGameplayAbility_Interaction::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// 시작은 알렸는데 결과 없이 끝나는 경우 = 몽타주 중단, 입력 취소, 플레이어 사망 등
	if (IsActive() && ActorInfo->IsNetAuthority() && bInteractionBegun && !bInteractionResolved)
	{
		if (AActor* Target = CurrentTarget.Get())
		{
			ITBInteractable::Execute_CancelInteract(Target, ActorInfo->AvatarActor.Get());
		}
	}
	
	bInteractionBegun = false;
	bInteractionResolved = false;
	
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	// 취소를 EndAbility에서 감지하는 이유 : 외부에서 CancelAbility호출, 플레이어 사망까지 모든 종료 경로가 EndAbility를 지나가기때문
}

void UTBGameplayAbility_Interaction::HandleActionMontageCompleted()
{
    AActor* Target = CurrentTarget.Get();

    // 흡혈: 몽타주 완료 시 결과 (서버만)
	if (CurrentActorInfo && CurrentActorInfo->IsNetAuthority() && Target && !ITBInteractable::Execute_ShouldResolveOnStart(Target))
    {
        ResolveInteraction();
    }

    Super::HandleActionMontageCompleted();
}

void UTBGameplayAbility_Interaction::ResolveInteraction()
{
    AActor* Target = CurrentTarget.Get();
    AActor* Avatar = GetAvatarActorFromActorInfo();     // 매개변수 없이 Avatar 얻는 함수
    UTBInteractionComponent* InteractionComp =
        Avatar ? Avatar->FindComponentByClass<UTBInteractionComponent>() : nullptr;

    if (!Target || !InteractionComp)
    {
        return;
    }

    // [서버] 게임 상태 결과
    ITBInteractable::Execute_Interact(Target, Avatar);

    // [서버 → 그 플레이어] 로컬 결과 (대화 자막 등)
    InteractionComp->Client_NotifyInteracted(Target);
}
