#pragma once

#include "AbilitySystem/Abilities/Monster/TBGameplayAbility_MonsterSkill.h"
#include "Components/SkeletalMeshComponent.h"
#include "TBGameplayAbility_Monster_Leap.generated.h"

class ATBMonster;
class AActor;
class USkeletalMeshComponent;
class UPrimitiveComponent;

// 몬스터 전용 서버 도약 및 착지 공격 어빌리티
UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_Monster_Leap : public UTBGameplayAbility_MonsterSkill
{
	GENERATED_BODY()

public:
	UTBGameplayAbility_Monster_Leap();

	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	void LaunchLeap();
	// Shared spatial and state checks for BT decision making and activation.
	bool CanLeapAtTarget(const ATBMonster* Monster, const AActor* Target) const;
	virtual bool CanUseAgainst(const ATBMonster* Monster, const AActor* Target) const override
	{
		return CanLeapAtTarget(Monster, Target);
	}

protected:
	virtual void HandleActionMontageCompleted() override;

	UPROPERTY(EditDefaultsOnly, Category = "Leap", meta = (ClampMin = "1.0", Units = "cm"))
	float MaxLeapRange = 700.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Leap", meta = (ClampMin = "0.1", Units = "s"))
	float FlightTime = 0.65f;

	// Maximum time from activation through windup, flight and recovery.
	UPROPERTY(EditDefaultsOnly, Category = "Leap", meta = (ClampMin = "0.1", Units = "s"))
	float MaxActiveDuration = 10.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Leap", meta = (ClampMin = "0.0"))
	float AttackDamage = 30.0f;

	// Slow horizontal movement near the leap target.
	UPROPERTY(EditDefaultsOnly, Category = "Leap|Movement", meta = (ClampMin = "0.0", Units = "cm", DisplayName = "Slowdown Distance"))
	float HomingStopDistance = 150.0f;

private:
	void EndLeap(bool bCancelled);
	void HandleAbilityTimeout();
	void StopLeapTracking();
	void CheckLeapContacts();
	void ApplyLeapContactDamage(AActor* OtherActor, const FHitResult& Hit);

	UFUNCTION()
	void UpdateLeapMovement(float DeltaSeconds, FVector OldLocation, FVector OldVelocity);

	UFUNCTION()
	void HandleLeapHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void HandleLeapOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleLanded(const FHitResult& Hit);

	// 도약 중 시전자와 접촉 판정 대상
	TWeakObjectPtr<ATBMonster> LeapingMonster;
	TWeakObjectPtr<const AActor> LeapTarget;
	float LeapHorizontalSpeed = 0.f;
	TWeakObjectPtr<USkeletalMeshComponent> AttackMesh;
	EVisibilityBasedAnimTickOption PreviousTickOption{};
	FTimerHandle AbilityTimeout;
	FTimerHandle ContactTimer;
	TSet<TWeakObjectPtr<AActor>> DamagedActors;
	bool bLaunched = false;
	bool bLanded = false;
	bool bMontageCompleted = false;
	bool bUsingAttackRotation = false;
	bool bPreviousOrientRotationToMovement = false;
	bool bPreviousUseControllerDesiredRotation = false;
	bool bPreviousUseControllerRotationYaw = false;
};
