#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/TBGameplayAbility_Attack.h"
#include "Components/SkinnedMeshComponent.h"
#include "Engine/HitResult.h"
#include "TBGameplayAbility_Melee.generated.h"

class AActor;
class UTBMeleeTraceComponent;
class USkeletalMeshComponent;

// 플레이어·몬스터 공통 근접 공격 어빌리티
UCLASS(Blueprintable)
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_Melee
	: public UTBGameplayAbility_Attack
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
	// 기본 공격을 사용할 때마다 순환할 몽타주 섹션
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Attack")
	TArray<FName> AttackSections;

	virtual FName GetActionMontageStartSection() const override;

private:
	// 서버에서 실제 재생한 타수를 소유 클라이언트에 전달
	UFUNCTION(Client, Reliable)
	void ClientConfirmAttackSection(AActor* Avatar, FPredictionKey PredictionKey, int32 PlayedIndex);

	// 같은 PlayerState에서 새 캐릭터로 교체된 경우 첫 타부터 시작
	TWeakObjectPtr<AActor> SequenceAvatar;
	int32 NextSectionIndex = 0;
	int32 LocalPlayedIndex = INDEX_NONE;
	
	void HandleMeleeHit(const FHitResult& HitResult);

	TWeakObjectPtr<UTBMeleeTraceComponent> ActiveTrace;
	TWeakObjectPtr<USkeletalMeshComponent> AttackMesh;

	FDelegateHandle HitDelegateHandle;

	// 공격 종료 후 복구할 애니메이션 갱신 설정
	EVisibilityBasedAnimTickOption PreviousTickOption =
		EVisibilityBasedAnimTickOption::AlwaysTickPose;
};
