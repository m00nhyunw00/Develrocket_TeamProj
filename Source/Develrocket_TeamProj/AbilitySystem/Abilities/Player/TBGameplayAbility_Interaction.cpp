#include "AbilitySystem/Abilities/Player/TBGameplayAbility_Interaction.h"

#include "TBGameplayTags.h"
#include "Components/TBInteractionComponent.h"
#include "Interface/TBInteractable.h"

UTBGameplayAbility_Interaction::UTBGameplayAbility_Interaction()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	
	SetAssetTags(FGameplayTagContainer(TBGameplayTags::Ability_Interaction));
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Dead);
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
	
	// 5. 대화 : 시작 시 결과 (서버만)
	if (HasAuthority(&ActivationInfo)&&ITBInteractable::Execute_ShouldResolveOnStart(Target))
	{
		ResolveInteraction();
	}
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
