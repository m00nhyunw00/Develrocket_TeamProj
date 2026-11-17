#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/Player/TBGameplayAbility_Interaction.h"
#include "TBGameplayAbility_Drain.generated.h"

UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_Drain : public UTBGameplayAbility_Interaction
{
	GENERATED_BODY()

public:
	UTBGameplayAbility_Drain();

protected:
	// 흡혈하는 플레이어에게 적용할 상태 GE (State.Draining, State.MovementBlocked 부여)
	UPROPERTY(EditDefaultsOnly, Category = "Interaction|Drain")
	TSubclassOf<UGameplayEffect> DrainStateEffectClass;

	virtual bool CanStartInteraction(const FGameplayAbilityActorInfo* ActorInfo, AActor* Target) const override;
	virtual bool PrepareInteraction(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, AActor* Target) override;
	virtual void CleanupInteraction(const FGameplayAbilityActorInfo* ActorInfo, AActor* Target) override;

private:
	// 흡혈 상태 적용 및 기존 전투·이동 입력 정리
	bool ApplyDrainState(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo);

	// 이번 상호작용에서 적용한 상태 GE 핸들
	FActiveGameplayEffectHandle DrainStateEffectHandle;

	// 상태 GE를 적용한 ASC
	TWeakObjectPtr<UAbilitySystemComponent> DrainStateASC;
};