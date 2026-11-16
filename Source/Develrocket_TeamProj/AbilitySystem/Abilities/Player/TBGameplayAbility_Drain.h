#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/ABilities/Player/TBGameplayAbility_Interaction.h"
#include "TBGameplayAbility_Drain.generated.h"

UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_Drain : public UTBGameplayAbility_Interaction
{
	GENERATED_BODY()
	
protected:
	// 흡혈하는 플레이어에게 적용할 상태 관리 GE
	UPROPERTY(EditDefaultsOnly, Category = "Interaction|Drain")
	TSubclassOf<UGameplayEffect> DrainStateEffectClass;
	
	virtual bool CanStartInteraction(const FGameplayAbilityActorInfo* ActorInfo, AActor* Target) const override;
	virtual bool PrepareInteraction(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, AActor* Target) override;
	virtual void CleanupInteraction(const FGameplayAbilityActorInfo* ActorInfo) override;
	
private:
	// 흡혈 상태 적용 및 기존 전투, 이동 입력 정리
	bool ApplyDrainState(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo);
	
	FActiveGameplayEffectHandle DrainStateEffectHandle;
	
	TWeakObjectPtr<UAbilitySystemComponent> DrainStateASC;
	
};