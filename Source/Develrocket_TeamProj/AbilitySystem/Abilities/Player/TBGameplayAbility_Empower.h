#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/TBGameplayAbility_Action.h"
#include "TimerManager.h"
#include "TBGameplayAbility_Empower.generated.h"

class UGameplayEffect;
struct FGameplayEffectRemovalInfo;

UCLASS(Blueprintable)
class DEVELROCKET_TEAMPROJ_API UTBGameplayAbility_Empower : public UTBGameplayAbility_Action
{
    GENERATED_BODY()

public:
    UTBGameplayAbility_Empower();

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

    // 현재 남은 시간에 따른 피해 감소율
    float GetDamageReduction() const;

    // 공격 피해량에 배율을 반영하고 강화 종료
    bool ConsumeForAttack(float& InOutDamage);

protected:
    UPROPERTY(EditDefaultsOnly, Category = "Empower")
    TSubclassOf<UGameplayEffect> BuffEffectClass;

    UPROPERTY(EditDefaultsOnly, Category = "Empower", meta = (ClampMin = "0.0"))
    float InitialSpeedBonus = 0.3f;

    UPROPERTY(EditDefaultsOnly, Category = "Empower", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float InitialDamageReduction = 0.3f;

    UPROPERTY(EditDefaultsOnly, Category = "Empower", meta = (ClampMin = "1.0"))
    float AttackDamageMultiplier = 2.0f;

    virtual bool ShouldWaitForMontageEnd() const override
    {
        return false;
    }

private:
    FActiveGameplayEffectHandle BuffHandle;
    FTimerHandle UpdateTimer;

    float GetRemainingRatio() const;
    void UpdateSpeedBonus();
    void OnBuffRemoved(const FGameplayEffectRemovalInfo& RemovalInfo);

    UFUNCTION()
    void OnInterrupted();
};
