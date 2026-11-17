#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/TBGameplayAbility_Action.h"
#include "TBGameplayAbility_Interaction.generated.h"

UCLASS(Abstract)
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_Interaction : public UTBGameplayAbility_Action
{
	GENERATED_BODY()

public:
	UTBGameplayAbility_Interaction();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:	
	virtual void HandleActionMontageCompleted() override;
	
	// 시작 전 추가 조건 검사, false면 상호작용 취소
	virtual bool CanStartInteraction(const FGameplayAbilityActorInfo* ActorInfo, AActor* Target) const;
	
	// 몽타주 재생 직전 준비 (상태 적용, 정렬 등), false면 상호작용 취소
	virtual bool PrepareInteraction(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, AActor* Target);
	
	// 종료 시 정리 * 성공, 취소, 준비 등 어느 경우에 불려도 안전해야함
	virtual void CleanupInteraction(const FGameplayAbilityActorInfo* ActorInfo, AActor* Target);

private:
	// [서버] Execute_Interact + Client_NotifyInteracted
	void ResolveInteraction();

	// 완료 시점까지 기억할 대상
	TWeakObjectPtr<AActor> CurrentTarget;
	
	// [서버] BeginInteract를 보냈는지, 결과 (Interact)까지 갔는지 -> 취소 알림이 필요한지 판단.
	bool bInteractionBegun = false;
	bool bInteractionResolved = false;
};