#pragma once

#include "AbilitySystem/Abilities/TBGameplayAbility_Action.h"
#include "Components/SkeletalMeshComponent.h"
#include "TBGameplayAbility_Monster_Leap.generated.h"

class ATBMonster;
class AActor;
class USkeletalMeshComponent;
class UPrimitiveComponent;

// 몬스터 전용 서버 도약 및 착지 공격 어빌리티
UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_Monster_Leap : public UTBGameplayAbility_Action
{
	GENERATED_BODY()

public:
	UTBGameplayAbility_Monster_Leap();

	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	void LaunchLeap();
	// Shared spatial and state checks for BT decision making and activation.
	bool CanLeapAtTarget(const ATBMonster* Monster, const AActor* Target) const;

protected:
	virtual void HandleActionMontageCompleted() override;

	UPROPERTY(EditDefaultsOnly, Category = "Leap", meta = (ClampMin = "1.0", Units = "cm"))
	float MaxLeapRange = 700.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Leap", meta = (ClampMin = "0.1", Units = "s"))
	float FlightTime = 0.65f;

	UPROPERTY(EditDefaultsOnly, Category = "Leap", meta = (ClampMin = "0.0"))
	float AttackDamage = 30.0f;

	// Maximum horizontal steering speed; zero disables homing.
	UPROPERTY(EditDefaultsOnly, Category = "Leap|Homing", meta = (ClampMin = "0.0", Units = "deg/s"))
	float HomingTurnRate = 90.0f;

	// Stop steering near the target to prevent abrupt turns or circling.
	UPROPERTY(EditDefaultsOnly, Category = "Leap|Homing", meta = (ClampMin = "0.0", Units = "cm"))
	float HomingStopDistance = 150.0f;

private:
	void EndLeap(bool bCancelled);
	void HandleLandingTimeout();
	void UpdateLeapHoming();
	void CheckLeapMeshContacts();
	void ApplyLeapContactDamage(AActor* OtherActor, const FHitResult& Hit);

	UFUNCTION()
	void HandleLeapHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void HandleLeapOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleLanded(const FHitResult& Hit);

	// 한 번의 도약 동안 추적하는 시전자와 발동 시점의 목표 위치
	TWeakObjectPtr<ATBMonster> LeapingMonster;
	TWeakObjectPtr<const AActor> LeapTarget;
	FTimerHandle HomingTimer;
	double LastHomingUpdateTime = 0.0;
	FVector LeapDestination = FVector::ZeroVector;
	TWeakObjectPtr<USkeletalMeshComponent> AttackMesh;
	EVisibilityBasedAnimTickOption PreviousTickOption{};
	FTimerHandle LandingTimeout;
	FTimerHandle ContactTimer;
	TSet<TWeakObjectPtr<AActor>> DamagedActors;
	bool bLaunched = false;
	bool bLanded = false;
	bool bMontageCompleted = false;
};
