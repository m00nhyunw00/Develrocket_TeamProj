#include "AbilitySystem/Abilities/Player/TBGameplayAbility_Empower.h"

#include "Abilities/Tasks/AbilityTask_WaitGameplayTag.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "TBGameplayTags.h"

UTBGameplayAbility_Empower::UTBGameplayAbility_Empower()
{
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

    SetAssetTags(FGameplayTagContainer(
        TBGameplayTags::Ability_Combat_Skill2));

    ActivationBlockedTags.AddTag(TBGameplayTags::State_Dead);
    ActivationBlockedTags.AddTag(TBGameplayTags::State_Stunned);
    ActivationBlockedTags.AddTag(TBGameplayTags::State_Draining);
}

void UTBGameplayAbility_Empower::ActivateAbility(
    FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    const UGameplayEffect* Effect = BuffEffectClass ? BuffEffectClass.GetDefaultObject() : nullptr;

    if (!ActorInfo || !ActorInfo->IsNetAuthority() || !ActorInfo->AbilitySystemComponent.IsValid()
        || !Effect
        || Effect->DurationPolicy != EGameplayEffectDurationType::HasDuration
        || !FMath::IsFinite(InitialSpeedBonus) || InitialSpeedBonus < 0.0f
        || !FMath::IsFinite(InitialDamageReduction)
        || InitialDamageReduction < 0.0f || InitialDamageReduction > 1.0f
        || !FMath::IsFinite(AttackDamageMultiplier)
        || AttackDamageMultiplier < 1.0f)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 비용과 쿨다운을 적용하기 전에 강화 GE의 지속 시간 검증
    FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(BuffEffectClass, GetAbilityLevel());
    const float BuffDuration = Spec.IsValid() ? Spec.Data->GetDuration() : 0.0f;
    if (!FMath::IsFinite(BuffDuration) || BuffDuration <= 0.0f)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    if (!IsActive())
    {
        return;
    }

    Spec.Data->SetSetByCallerMagnitude(TBGameplayTags::Data_MoveSpeedBonus, InitialSpeedBonus);
    BuffHandle = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);

    if (!BuffHandle.IsValid())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();

    if (auto* RemovedDelegate = ASC->OnGameplayEffectRemoved_InfoDelegate(BuffHandle))
    {
        RemovedDelegate->AddUObject(this, &ThisClass::OnBuffRemoved);
    }

    // 활성 중에만 서버에서 이동 속도 보너스 갱신
    GetWorld()->GetTimerManager().SetTimer(UpdateTimer, this, &ThisClass::UpdateSpeedBonus, 0.1f, true);

    const FGameplayTag InterruptTags[] = {
        TBGameplayTags::State_Dead,
        TBGameplayTags::State_Stunned,
        TBGameplayTags::State_Draining
    };

    for (const FGameplayTag& Tag : InterruptTags)
    {
        if (!IsActive())
        {
            return;
        }

        auto* Task = UAbilityTask_WaitGameplayTagAdded::WaitGameplayTagAdd(this, Tag, nullptr, true);

        Task->Added.AddDynamic(this, &ThisClass::OnInterrupted);
        Task->ReadyForActivation();
    }
}

float UTBGameplayAbility_Empower::GetRemainingRatio() const
{
    const UWorld* World = GetWorld();
    const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    if (!IsActive() || !World || !IsValid(ASC) || !BuffHandle.IsValid())
    {
        return 0.0f;
    }

    // 이번 강화에 적용된 효과의 실제 지속 시간과 남은 시간 조회
    const FActiveGameplayEffect* ActiveBuff = ASC->GetActiveGameplayEffect(BuffHandle);
    if (!ActiveBuff)
    {
        return 0.0f;
    }

    const float TotalDuration = ActiveBuff->GetDuration();
    if (!FMath::IsFinite(TotalDuration) || TotalDuration <= 0.0f)
    {
        return 0.0f;
    }

    const float RemainingTime = ActiveBuff->GetTimeRemaining(World->GetTimeSeconds());
    return FMath::Clamp(RemainingTime / TotalDuration, 0.0f, 1.0f);
}

float UTBGameplayAbility_Empower::GetDamageReduction() const
{
    return InitialDamageReduction * GetRemainingRatio();
}

void UTBGameplayAbility_Empower::UpdateSpeedBonus()
{
    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();

    if (!IsActive() || !IsValid(ASC) || !BuffHandle.IsValid())
    {
        return;
    }

    ASC->UpdateActiveGameplayEffectSetByCallerMagnitude(BuffHandle,TBGameplayTags::Data_MoveSpeedBonus,InitialSpeedBonus * GetRemainingRatio());
}

bool UTBGameplayAbility_Empower::ConsumeForAttack(float& InOutDamage)
{
    if (!CurrentActorInfo || !CurrentActorInfo->IsNetAuthority()
        || GetRemainingRatio() <= 0.0f
        || !FMath::IsFinite(InOutDamage) || InOutDamage <= 0.0f)
    {
        return false;
    }

    const float EnhancedDamage = InOutDamage * AttackDamageMultiplier;

    if (!FMath::IsFinite(EnhancedDamage))
    {
        return false;
    }

    // 버프 종료 전에 이번 공격에 사용할 피해량 확정
    InOutDamage = EnhancedDamage;

    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo,true, false);

    return true;
}

void UTBGameplayAbility_Empower::OnBuffRemoved(const FGameplayEffectRemovalInfo& RemovalInfo)
{
    if (IsActive())
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo,true, false);
    }
}

void UTBGameplayAbility_Empower::OnInterrupted()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo,true, true);
}

void UTBGameplayAbility_Empower::EndAbility(
    FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility,
    bool bWasCancelled)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(UpdateTimer);
    }

    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    const FActiveGameplayEffectHandle EffectToRemove = BuffHandle;
    BuffHandle.Invalidate();

    if (IsValid(ASC) && EffectToRemove.IsValid())
    {
        // 직접 제거할 때 종료 콜백의 재진입 방지
        if (auto* RemovedDelegate = ASC->OnGameplayEffectRemoved_InfoDelegate(EffectToRemove))
        {
            RemovedDelegate->RemoveAll(this);
        }

        ASC->RemoveActiveGameplayEffect(EffectToRemove);
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo,bReplicateEndAbility, bWasCancelled);
}
