#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/TBGameplayAbility_Action.h"
#include "GameplayTagContainer.h"
#include "Engine/HitResult.h"
#include "TBGameplayAbility_Attack.generated.h"

UCLASS(Abstract, Blueprintable)
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_Attack
    : public UTBGameplayAbility_Action
{
    GENERATED_BODY()

public:
    UTBGameplayAbility_Attack();

    virtual void ActivateAbility(
        FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    // 활성화 과정에서 발사 행동을 완료하는 공격인지 조회
    virtual bool CompletesAttackActionDuringActivation() const
    {
        return false;
    }

    // 어빌리티 종료와 별개인 공격 행동 완료 상태
    UFUNCTION(BlueprintPure, Category = "Combat")
    bool HasCompletedAttackAction() const
    {
        return bAttackActionCompleted;
    }

protected:
    // 기본 공격·F키 공격·스킬1·스킬2 등의 기능 선택
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Attack",
        meta = (Categories = "Ability.Combat"))
    FGameplayTag AttackId;

    // 이번 실행에서 서버가 확정한 피해량
    UPROPERTY(Transient)
    float CurrentAttackDamage = 0.0f;

    // 피해를 허용할 대상 클래스
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Targeting")
    TArray<TSubclassOf<AActor>> AllowedTargetClasses;

    // 기존 PlayerState의 기본 공격 간격·쿨다운 GE 사용 여부
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Cooldown")
    bool bUsePlayerPrimaryAttackCooldown = false;

    // 명중 시에도 공격자의 행동 가능 상태가 필요한지 조회
    virtual bool RequiresUnblockedAttackerOnHit() const
    {
        return true;
    }

    // 공통 명중 대상 검증
    bool IsAllowedAttackHit(const FHitResult& HitResult) const;

    // 발사 또는 공격 행동 완료 기록
    void MarkAttackActionCompleted()
    {
        bAttackActionCompleted = true;
    }

    virtual const FGameplayTagContainer* GetCooldownTags() const override;

    virtual void ApplyCooldown(
        FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        FGameplayAbilityActivationInfo ActivationInfo) const override;

private:
    bool bAttackActionCompleted = false;
};