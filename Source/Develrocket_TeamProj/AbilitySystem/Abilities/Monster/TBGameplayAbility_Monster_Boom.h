#pragma once

#include "AbilitySystem/Abilities/Monster/TBGameplayAbility_MonsterSkill.h"
#include "Components/SkinnedMeshComponent.h"
#include "TBGameplayAbility_Monster_Boom.generated.h"

class UAnimSequenceBase;
class UNiagaraSystem;

UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_Monster_Boom : public UTBGameplayAbility_MonsterSkill
{
	GENERATED_BODY()

public:
	UTBGameplayAbility_Monster_Boom();

	virtual bool CanUseAgainst(const ATBMonster* Monster, const AActor* Target) const override;
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	void JumpForward(const UAnimSequenceBase* Animation);

protected:
	// 블렌드아웃 중 몽타주 중단도 도약을 취소한다.
	virtual bool ShouldAllowMontageInterruptAfterBlendOut() const override { return true; }
	virtual void HandleActionMontageCompleted() override;

	UPROPERTY(EditDefaultsOnly, Category = "Boom|Jump", meta = (ClampMin = "0.05", Units = "s"))
	float JumpTravelTime = 0.55f;

	UPROPERTY(EditDefaultsOnly, Category = "Boom", meta = (ClampMin = "1.0", Units = "cm"))
	float ExplosionRadius = 500.f;

	UPROPERTY(EditDefaultsOnly, Category = "Boom", meta = (ClampMin = "0.0"))
	float ExplosionDamage = 40.f;

	UPROPERTY(EditDefaultsOnly, Category = "Boom", meta = (ToolTip = "One-shot Niagara system played at the explosion location."))
	TObjectPtr<UNiagaraSystem> ExplosionEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Boom", meta = (ToolTip = "Spawns one actor per entry at the explosion transform. Enable replication on actor classes that clients need to see."))
	TArray<TSubclassOf<AActor>> SpawnActorClasses;

private:
	UFUNCTION()
	void HandleMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity);

	UFUNCTION()
	void HandleLanded(const FHitResult& Hit);

	void Detonate(ATBMonster& Monster);

	EVisibilityBasedAnimTickOption OriginalAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	bool bSavedAnimTickOption = false;
	bool bDetonated = false;
	bool bJumped = false;
	bool bLanded = false;
};
