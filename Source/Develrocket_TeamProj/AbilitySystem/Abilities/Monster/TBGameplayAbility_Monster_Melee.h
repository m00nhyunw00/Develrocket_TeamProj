#pragma once

#include "CoreMinimal.h"
#include "Components/SkinnedMeshComponent.h"
#include "AbilitySystem/Abilities/TBGameplayAbility_Action.h"
#include "TBGameplayAbility_Monster_Melee.generated.h"

// Montage-based melee ability. Damage and hit detection are supplied by its implementation.
UCLASS(Blueprintable)
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_Monster_Melee : public UTBGameplayAbility_Action
{
	GENERATED_BODY()

public:
	UTBGameplayAbility_Monster_Melee();
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	void BeginHitWindow();
	void EndHitWindow();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Monster|Attack", meta = (ClampMin = "0.0"))
	float AttackDamage = 20.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Monster|Attack")
	TArray<FName> AttackBodyBones = { TEXT("jt_FrontLeg1_R4"), TEXT("jt_FrontLeg1_L4") };

private:
	void CheckHitWindow();
	FTimerHandle HitTimer;
	TSet<TWeakObjectPtr<AActor>> HitActors;
	TWeakObjectPtr<USkeletalMeshComponent> AttackMesh;
	EVisibilityBasedAnimTickOption PreviousTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose;
};
