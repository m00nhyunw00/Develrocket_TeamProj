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
	virtual bool CanStartInteraction(const FGameplayAbilityActorInfo* ActorInfo, AActor* Target) const override;
	virtual bool PrepareInteraction(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, AActor* Target) override;
	

private:
	// 흡혈 시작 전 기존 전투 어빌리티 취소와 이동 입력 정리
	bool PrepareDrainInput(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo);
};