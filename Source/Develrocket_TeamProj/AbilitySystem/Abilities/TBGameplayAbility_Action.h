#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "TBGameplayAbility_Action.generated.h"

class UAnimInstance;
class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;

UCLASS(Abstract)
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_Action : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UTBGameplayAbility_Action();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	// 비용 적용 시점 ----------------------

	UPROPERTY(EditDefaultsOnly, Category = "Cost")
	bool bCommitOnActivate = true;

	// 몽타주 ----------------------

	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> ThirdPersonMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> FirstPersonMontage;

	// 자식 클래스의 완료·중단 처리 ----------------------

	virtual void HandleActionMontageCompleted();
	virtual void HandleActionMontageInterrupted();
	
	// 몽타주 완료까지 Ability를 유지할지 여부 ----------------------
	virtual bool ShouldWaitForMontageEnd() const
	{
		return true;
	}

	// 로컬 플레이어의 선택적 1인칭 연출 ----------------------

	virtual UAnimInstance* GetFirstPersonAnimInstance() const;

private:
	// 몽타주 태스크 콜백 ----------------------

	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageInterrupted();

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;
};