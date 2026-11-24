#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "TBGameplayAbility_Action.generated.h"

class UAnimInstance;
class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitDelay;

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

	// 선택적 몽타주: 둘 다 없으면 자식에서 실행 완료 및 종료 처리 ----------------------

	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> ThirdPersonMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> FirstPersonMontage;
	
	// 비용·쿨다운 적용 성공 후, 몽타주 재생 전 확장 지점 ----------------------
	virtual void OnActionCommitted(){}

	// 자식 클래스의 완료·중단 처리 ----------------------

	virtual void HandleActionMontageCompleted();
	virtual void HandleActionMontageInterrupted();
	
	// 몽타주 재생중 끊길 시 재생할 가상함수 ----------------------
	virtual bool ShouldAllowMontageInterruptAfterBlendOut() const { return false; }
	
	// 3인칭 우선, 1인칭만 있으면 해당 몽타주의 완료 대기 ----------------------
	// 몽타주가 없거나 false인 경우 자식에서 EndAbility 호출
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

	// 실제로 재생한 1인칭 몽타주 인스턴스의 완료·중단 처리
	void OnFirstPersonMontageEnded(UAnimMontage* Montage, bool bInterrupted, int32 MontageInstanceID);
	void OnFirstPersonMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted, int32 MontageInstanceID);

	TWeakObjectPtr<UAnimInstance> PlayingFirstPersonAnimInstance;
	int32 FirstPersonMontageInstanceID = INDEX_NONE;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	// 1인칭 메시를 재생하지 않는 서버의 단발 몽타주 완료 대기
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitDelay> FirstPersonWaitTask;
};
