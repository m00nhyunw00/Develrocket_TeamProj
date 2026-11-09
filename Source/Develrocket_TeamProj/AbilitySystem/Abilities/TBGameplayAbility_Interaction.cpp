#include "AbilitySystem/Abilities/TBGameplayAbility_Interaction.h"

#include "TBGameplayTags.h"

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
	
	// 1단계 확인용: Avatar(실제 캐릭터) 이름 출력
	UE_LOG(LogTemp, Log, TEXT("[Interaction] Activated by %s"),
		*GetNameSafe(GetAvatarActorFromActorInfo()));
	
	// 2단계: 감지 컴포넌트에서 대상을 찾아 Execute_Interact 호출
	
	// 즉시 종료해야 다음 F 입력에서 다시 실행가능
	// (beReplicateEndAbility, bWasCancelled) -> 정상종료이므로 취소 아님
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
