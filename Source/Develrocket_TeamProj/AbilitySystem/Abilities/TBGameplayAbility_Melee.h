#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/TBGameplayAbility_Action.h"
#include "Components/SkinnedMeshComponent.h"
#include "Engine/HitResult.h"
#include "TBGameplayAbility_Melee.generated.h"

class AActor;
class UTBMeleeTraceComponent;
class USkeletalMeshComponent;

// 플레이어·몬스터 공통 근접 공격 어빌리티
UCLASS(Blueprintable)
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_Melee
	: public UTBGameplayAbility_Action
{
	GENERATED_BODY()

public:
	UTBGameplayAbility_Melee();

	virtual void ActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Melee")
	FGameplayTag DamageTypeTag;

	// 이 공격으로 피해를 줄 수 있는 대상 클래스
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Melee")
	TArray<TSubclassOf<AActor>> AllowedTargetClasses;

private:
	void HandleMeleeHit(const FHitResult& HitResult);

	TWeakObjectPtr<UTBMeleeTraceComponent> ActiveTrace;
	TWeakObjectPtr<USkeletalMeshComponent> AttackMesh;

	FDelegateHandle HitDelegateHandle;

	// 공격 종료 후 복구할 애니메이션 갱신 설정
	EVisibilityBasedAnimTickOption PreviousTickOption =
		EVisibilityBasedAnimTickOption::AlwaysTickPose;
};