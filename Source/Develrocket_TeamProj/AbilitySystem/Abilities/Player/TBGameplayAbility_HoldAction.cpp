#include "AbilitySystem/Abilities/Player/TBGameplayAbility_HoldAction.h"

#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayTag.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/TBAttributeSetBase.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "TBGameplayTags.h"

UTBGameplayAbility_HoldAction::UTBGameplayAbility_HoldAction()
{
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    ActivationBlockedTags.AddTag(TBGameplayTags::State_Dead);
    ActivationBlockedTags.AddTag(TBGameplayTags::State_Stunned);
    ActivationBlockedTags.AddTag(TBGameplayTags::State_Draining);
}

void UTBGameplayAbility_HoldAction::ActivateAbility(
    FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

    const UGameplayEffect* StateEffect = StateEffectClass ? StateEffectClass.GetDefaultObject() : nullptr;

    const UGameplayEffect* CostEffect = PeriodicCostEffectClass
        ? PeriodicCostEffectClass.GetDefaultObject()
        : nullptr;

    if (!IsValid(ASC) || !StateEffect
        || StateEffect->DurationPolicy != EGameplayEffectDurationType::Infinite
        || (CostEffect && CostEffect->DurationPolicy != EGameplayEffectDurationType::Instant))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    if (CostEffect && ASC->GetNumericAttribute(UTBAttributeSetBase::GetBloodAttribute()) <= 1.0f)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    if (!IsActive())
    {
        return;
    }

    // 소유 클라이언트의 예측 적용 및 서버 확정
    StateEffectHandle = ApplyGameplayEffectToOwner(Handle, ActorInfo, ActivationInfo, StateEffect, GetAbilityLevel());

    if (!StateEffectHandle.IsValid())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 반복 자원 소모는 서버에서만 실행
    if (CostEffect && ActorInfo->IsNetAuthority())
    {
        GetWorld()->GetTimerManager().SetTimer(CostTimer, this, &ThisClass::ApplyPeriodicCost, 1.0f, true);
    }

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

    if (!IsActive())
    {
        return;
    }

    auto* ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);

    ReleaseTask->OnRelease.AddDynamic(this, &ThisClass::OnInputReleased);
    ReleaseTask->ReadyForActivation();
}

void UTBGameplayAbility_HoldAction::ApplyPeriodicCost()
{
    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();

    if (!IsActive() || !CurrentActorInfo || !CurrentActorInfo->IsNetAuthority() || !IsValid(ASC))
    {
        return;
    }

    if (ASC->GetNumericAttribute(UTBAttributeSetBase::GetBloodAttribute()) <= 1.0f)
    {
        OnInterrupted();
        return;
    }

    const FActiveGameplayEffectHandle Result = ApplyGameplayEffectToOwner(
        CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo,
        PeriodicCostEffectClass.GetDefaultObject(), GetAbilityLevel());

    if (!Result.WasSuccessfullyApplied() || ASC->GetNumericAttribute(UTBAttributeSetBase::GetBloodAttribute()) <= 1.0f)
    {
        if (IsActive())
        {
            OnInterrupted();
        }
    }
}

void UTBGameplayAbility_HoldAction::OnInputReleased(float TimeHeld)
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo,true, false);
}

void UTBGameplayAbility_HoldAction::OnInterrupted()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo,true, true);
}

void UTBGameplayAbility_HoldAction::EndAbility(
    FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility,
    bool bWasCancelled)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(CostTimer);
    }

    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
    {
        if (StateEffectHandle.IsValid())
        {
            ASC->RemoveActiveGameplayEffect(StateEffectHandle);
        }
    }

    StateEffectHandle.Invalidate();

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}