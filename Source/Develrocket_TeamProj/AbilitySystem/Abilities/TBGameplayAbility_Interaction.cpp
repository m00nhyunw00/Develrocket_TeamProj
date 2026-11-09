#include "AbilitySystem/Abilities/TBGameplayAbility_Interaction.h"

#include "TBGameplayTags.h"
#include "Components/TBInteractionComponent.h"
#include "Interface/TBInteractable.h"

UTBGameplayAbility_Interaction::UTBGameplayAbility_Interaction()
{
	// 플레이어(ASC) 마다 인스턴스 하나를 만들어 재사용
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	
	// 세계관, 설명 UI는 누른 사람 화면에서만 열리면 되므로 로컬 전용
	// (게임 시작 처럼 서버 상태를 바꾸는 일은 GA가 아니라 LevelManager 경로로 보냄)
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
	
	// 이 GA의 이름표: 입력에서 이 태그를 찾음
	SetAssetTags(FGameplayTagContainer(TBGameplayTags::Ability_Interaction));
	
	// 죽은 상태에서는 상호작용 불가
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Dead);
}

void UTBGameplayAbility_Interaction::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// ① 실제 캐릭터(Avatar)
	AActor* Avatar = GetAvatarActorFromActorInfo();

	// ② 캐릭터에 붙은 정찰병 찾기
	const UTBInteractionComponent* InteractionComp =
		Avatar ? Avatar->FindComponentByClass<UTBInteractionComponent>() : nullptr;

	// ③ 정찰병이 지금 보고 있는 대상
	AActor* Target = InteractionComp ? InteractionComp->GetFocusedActor() : nullptr;

	// ④ 대상이 없으면 취소 종료
	if (!Target)
	{
		UE_LOG(LogTemp, Log, TEXT("[Interaction] No target"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// ⑤ 대상에게 상호작용 요청 (BP 구현까지 호출되도록 Execute_ 사용)
	UE_LOG(LogTemp, Log, TEXT("[Interaction] %s -> %s"), *GetNameSafe(Avatar), *GetNameSafe(Target));
	ITBInteractable::Execute_Interact(Target, Avatar);

	// ⑥ 정상 종료
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
