#pragma once

#include "AbilitySystem/Abilities/TBGameplayAbility_Action.h"
#include "TBGameplayAbility_MonsterSkill.generated.h"

class ATBMonster;

UCLASS(Abstract)
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_MonsterSkill : public UTBGameplayAbility_Action
{
	GENERATED_BODY()

public:
	virtual bool CanUseAgainst(const ATBMonster* Monster, const AActor* Target) const { return true; }
	virtual UGameplayEffect* GetCooldownGameplayEffect() const override;
	virtual const FGameplayTagContainer* GetCooldownTags() const override;
	virtual void ApplyCooldown(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo) const override;

protected:
	bool HasConfiguredSkillCooldown() const;

	// The shared effect receives its granted tags and duration from the spec at application time.
	UPROPERTY(EditDefaultsOnly, Category = "Cooldown")
	TSubclassOf<UGameplayEffect> SkillCooldownEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Cooldown", meta = (ClampMin = "0.0", Units = "s"))
	float SkillCooldownDuration = 0.f;
};
