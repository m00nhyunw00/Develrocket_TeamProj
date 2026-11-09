#include "AbilitySystem/Abilities/Player/TBGameplayAbility_Interaction.h"

#include "TBGameplayTags.h"
#include "Components/TBInteractionComponent.h"
#include "Interface/TBInteractable.h"

UTBGameplayAbility_Interaction::UTBGameplayAbility_Interaction()
{
	// 플레이어(ASC)마다 인스턴스 하나를 만들어 재사용
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// 판정과 실행은 서버에서만
	// 클라이언트에서 TryActivateAbilityByTag를 호출하면 GAS가 자동으로 서버에 활성화 요청을 보냄
	// 클라이언트 쪽 반응(UI/연출)은 서버 처리 후 Client RPC로 전달
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	// 이 GA의 이름표: 입력에서 이 태그로 찾음
	SetAssetTags(FGameplayTagContainer(TBGameplayTags::Ability_Interaction));

	// 죽은 상태에서는 상호작용 불가
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Dead);
}

void UTBGameplayAbility_Interaction::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 안전장치: ServerOnly 정책이지만 서버 권한이 없으면 아무것도 하지 않음
	if (!HasAuthority(&ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// ① 실제 캐릭터(Avatar)
	AActor* Avatar = GetAvatarActorFromActorInfo();

	// ② 캐릭터에 붙은 상호작용 컴포넌트 (Client RPC 호출이 필요하므로 non-const)
	UTBInteractionComponent* InteractionComp =
		Avatar ? Avatar->FindComponentByClass<UTBInteractionComponent>() : nullptr;

	// ③ 클라이언트의 FocusedActor를 믿지 않고, 서버가 직접 다시 탐지한 대상 사용 (치트 방지)
	AActor* Target = InteractionComp ? InteractionComp->FindTargetForServer() : nullptr;

	// ④ 대상이 없으면 취소 종료
	if (!Target)
	{
		UE_LOG(LogTemp, Log, TEXT("[Interaction] Server: No target (%s)"), *GetNameSafe(Avatar));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Interaction] Server: %s -> %s"), *GetNameSafe(Avatar), *GetNameSafe(Target));

	// ⑤ [서버] 게임 상태를 바꾸는 상호작용 (아이템 획득, 문 열기 등)
	ITBInteractable::Execute_Interact(Target, Avatar);

	// ⑥ [그 플레이어의 클라이언트] UI / 연출 등 로컬 반응
	InteractionComp->Client_NotifyInteracted(Target);

	// ⑦ 정상 종료
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}