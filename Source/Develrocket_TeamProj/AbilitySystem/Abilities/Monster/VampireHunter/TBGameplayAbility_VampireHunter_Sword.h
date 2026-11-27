#pragma once

#include "Abilities/GameplayAbility.h"
#include "AITypes.h"
#include "TBGameplayAbility_VampireHunter_Sword.generated.h"

class ATBMonster;
class USkeletalMeshComponent;
class UTBHunterCombatComponent;
class UTBMeleeTraceComponent;

UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_VampireHunter_Sword : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UTBGameplayAbility_VampireHunter_Sword();
	bool CanStrike(const ATBMonster& Hunter, const AActor& Victim) const;
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	enum class EStage : uint8 { Approach, FirstSwing, SecondSwing, Recovery };
	void UpdateAttack();
	void StartSwing(EStage Swing, ATBMonster& Hunter, const AActor& Victim, UTBHunterCombatComponent& Combat);
	void StartRecovery(ATBMonster& Hunter, const AActor& Victim, UTBHunterCombatComponent& Combat);
	void StopAttackMove(ATBMonster& Hunter);
	void HandleMeleeHit(const FHitResult& Hit);
	void FinishAttack(bool bCancelled);

	TWeakObjectPtr<AActor> Target;
	TWeakObjectPtr<USkeletalMeshComponent> AttackMesh;
	TWeakObjectPtr<UTBMeleeTraceComponent> ActiveTrace;
	FDelegateHandle TraceHitHandle;
	FTimerHandle AttackTimer;
	FAIRequestID AttackMoveId;
	double StageStartedAt = 0.0;
	float SwingDuration = 0.f;
	float SwingHitTime = 0.f;
	uint8 PreviousVisibilityTickOption = 0;
	EStage Stage = EStage::Approach;
	bool bApproachStrike = false;
	bool bTraceWindowOpen = false;
};
