#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "TBGameplayAbility_BloodAction.generated.h"

class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;

UCLASS(Abstract)
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_BloodAction : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UTBGameplayAbility_BloodAction();

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

private:
	// 몽타주 종료 처리 ----------------------

	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageInterrupted();

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;
};



UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_PrimaryAttack
	: public UTBGameplayAbility_BloodAction
{
	GENERATED_BODY()

public:
	UTBGameplayAbility_PrimaryAttack();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};



UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_Reload : public UTBGameplayAbility_BloodAction
{
	GENERATED_BODY()

public:
	UTBGameplayAbility_Reload();

	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	// 재장전 완료 시 실제 충전량을 비용 GE에 전달
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
};