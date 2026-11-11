#pragma once

#include "AbilitySystem/Abilities/TBGameplayAbility_Action.h"
#include "Components/SkeletalMeshComponent.h"
#include "TBGameplayAbility_Monster_Leap.generated.h"

class ATBMonster;
class ATBPlayer;
class USkeletalMeshComponent;

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

protected:
	virtual void HandleActionMontageCompleted() override;

	UPROPERTY(EditDefaultsOnly, Category = "Leap", meta = (ClampMin = "1.0", Units = "cm"))
	float MaxLeapRange = 700.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Leap", meta = (ClampMin = "0.1", Units = "s"))
	float FlightTime = 0.65f;

	UPROPERTY(EditDefaultsOnly, Category = "Leap", meta = (ClampMin = "1.0", Units = "cm"))
	float LandingRadius = 160.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Leap", meta = (ClampMin = "0.0"))
	float AttackDamage = 30.0f;

private:
	bool FindLandingLocation(const ATBMonster& Monster, const ATBPlayer& Player, FVector& Location) const;
	void EndLeap(bool bCancelled);
	void HandleLandingTimeout();

	UFUNCTION()
	void HandleLanded(const FHitResult& Hit);

	// 한 번의 도약 동안 추적하는 시전자와 BT 타깃
	TWeakObjectPtr<ATBMonster> LeapingMonster;
	TWeakObjectPtr<const ATBPlayer> LeapTarget;
	TWeakObjectPtr<USkeletalMeshComponent> AttackMesh;
	EVisibilityBasedAnimTickOption PreviousTickOption{};
	FTimerHandle LandingTimeout;
	bool bLaunched = false;
	bool bLanded = false;
	bool bMontageCompleted = false;
};
