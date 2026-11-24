#include "AbilitySystem/Abilities/TBGameplayAbility_Attack.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/TBCharacterBase.h"
#include "Character/Player/TBPlayer.h"
#include "Character/Monster/TBMonster.h"
#include "Character/Human/TBCitizen.h"
#include "Framework/TBPlayerState.h"
#include "GameplayEffect.h"
#include "Interface/TBHitInterface.h"
#include "TBGameplayTags.h"

UTBGameplayAbility_Attack::UTBGameplayAbility_Attack()
{
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    bCommitOnActivate = true;

    ActivationBlockedTags.AddTag(TBGameplayTags::State_Combat);
    ActivationBlockedTags.AddTag(TBGameplayTags::State_Dead);
    ActivationBlockedTags.AddTag(TBGameplayTags::State_Stunned);
    ActivationBlockedTags.AddTag(TBGameplayTags::State_Draining);

    AllowedTargetClasses = {
        ATBPlayer::StaticClass(),
        ATBMonster::StaticClass(),
        ATBCitizen::StaticClass()
    };
}

void UTBGameplayAbility_Attack::ActivateAbility(
    FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    bAttackActionCompleted = false;
    CurrentAttackDamage = 0.0f;

    ATBCharacterBase* Attacker = ActorInfo ? Cast<ATBCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;

    if (!IsValid(Attacker) || Attacker->IsActionBlocked()
        || !AttackId.IsValid()
        || !GetAssetTags().HasTagExact(AttackId)
        || AllowedTargetClasses.IsEmpty()
        || !bCommitOnActivate)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 기존 플레이어 기본 공격의 쿨다운 정책을 선택한 경우 검증
    if (bUsePlayerPrimaryAttackCooldown)
    {
        const ATBPlayerState* PS = Cast<ATBPlayerState>(ActorInfo->OwnerActor.Get());

        const float Interval = PS ? PS->GetPrimaryAttackInterval() : 0.0f;

        const TSubclassOf<UGameplayEffect> EffectClass = PS ? PS->GetPrimaryAttackCooldownEffectClass() : nullptr;

        const UGameplayEffect* Effect = EffectClass ? EffectClass.GetDefaultObject() : nullptr;

        if (AttackId != TBGameplayTags::Ability_Combat_PrimaryAttack
            || !IsValid(PS)
            || !FMath::IsFinite(Interval) || Interval <= 0.0f
            || !Effect
            || Effect->DurationPolicy != EGameplayEffectDurationType::HasDuration
            || !Effect->GetGrantedTags().HasTagExact(TBGameplayTags::Cooldown_PrimaryAttack))
        {
            EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
            return;
        }
    }

    // 기능별 피해량을 서버에서 한 번 조회하여 이번 실행에 보관
    if (ActorInfo->IsNetAuthority())
    {
        if (!Attacker->GetAttackDamage(AttackId, CurrentAttackDamage)
            || !FMath::IsFinite(CurrentAttackDamage)
            || CurrentAttackDamage <= 0.0f)
        {
            EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
            return;
        }
    }

    // 기존 Action의 비용·쿨다운 적용 및 선택적 몽타주 재생
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UTBGameplayAbility_Attack::OnActionCommitted()
{
    Super::OnActionCommitted();

    if (!CurrentActorInfo || !CurrentActorInfo->IsNetAuthority())
    {
        return;
    }

    ATBCharacterBase* Attacker = Cast<ATBCharacterBase>(GetAvatarActorFromActorInfo());

    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();

    if (!IsValid(Attacker) || !IsValid(ASC))
    {
        return;
    }

    // 피해량을 확정한 뒤 강화 상태 종료
    const bool bEnhanced = Attacker->ConsumeAttackEnhancement(AttackId, CurrentAttackDamage);

    if (AttackVisualCueTag.IsValid())
    {
        FGameplayCueParameters Parameters;
        Parameters.Instigator = Attacker;
        Parameters.EffectCauser = Attacker;

        // 버프가 사라진 이후에도 이번 공격의 연출 선택 정보 유지
        Parameters.NormalizedMagnitude = bEnhanced ? 1.0f : 0.0f;

        FScopedPredictionWindow CuePredictionWindow(ASC, FPredictionKey(), false);

        ASC->ExecuteGameplayCue(AttackVisualCueTag, Parameters);
    }
}

bool UTBGameplayAbility_Attack::IsAllowedAttackHit(const FHitResult& HitResult) const
{
    ATBCharacterBase* Attacker = Cast<ATBCharacterBase>(GetAvatarActorFromActorInfo());

    AActor* Target = HitResult.GetActor();

    if (!IsActive()
        || !FMath::IsFinite(CurrentAttackDamage)
        || CurrentAttackDamage <= 0.0f
        || !IsValid(Attacker) || !Attacker->HasAuthority()
        || !IsValid(Target) || Target == Attacker
        || !Target->Implements<UTBHitInterface>())
    {
        return false;
    }

    UAbilitySystemComponent* SourceASC = Attacker->GetAbilitySystemComponent();

    if (!IsValid(SourceASC)
        || SourceASC != GetAbilitySystemComponentFromActorInfo()
        || SourceASC->GetAvatarActor() != Attacker
        || SourceASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead))
    {
        return false;
    }

    if (RequiresUnblockedAttackerOnHit() && Attacker->IsActionBlocked())
    {
        return false;
    }

    const bool bAllowedTarget = AllowedTargetClasses.ContainsByPredicate(
        [Target](const TSubclassOf<AActor>& TargetClass)
        {
            return TargetClass.Get() && Target->IsA(TargetClass.Get());
        });

    if (!bAllowedTarget)
    {
        return false;
    }

    UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);

    return IsValid(TargetASC)
        && TargetASC->GetAvatarActor() == Target
        && !TargetASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead)
        && !TargetASC->HasMatchingGameplayTag(
            TBGameplayTags::State_Invulnerable);
}

const FGameplayTagContainer* UTBGameplayAbility_Attack::GetCooldownTags() const
{
    if (!bUsePlayerPrimaryAttackCooldown)
    {
        return Super::GetCooldownTags();
    }

    static const FGameplayTagContainer CooldownTags(TBGameplayTags::Cooldown_PrimaryAttack);

    return &CooldownTags;
}

void UTBGameplayAbility_Attack::ApplyCooldown(
    FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    FGameplayAbilityActivationInfo ActivationInfo) const
{
    if (!bUsePlayerPrimaryAttackCooldown)
    {
        Super::ApplyCooldown(Handle, ActorInfo, ActivationInfo);
        return;
    }

    const ATBPlayerState* PS = ActorInfo ? Cast<ATBPlayerState>(ActorInfo->OwnerActor.Get()) : nullptr;

    if (!IsValid(PS))
    {
        return;
    }

    const float Interval = PS->GetPrimaryAttackInterval();

    const TSubclassOf<UGameplayEffect> EffectClass = PS->GetPrimaryAttackCooldownEffectClass();

    if (!EffectClass || !FMath::IsFinite(Interval) || Interval <= 0.0f)
    {
        return;
    }

    FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo, EffectClass, GetAbilityLevel(Handle, ActorInfo));

    if (!Spec.IsValid())
    {
        return;
    }

    Spec.Data->SetDuration(Interval, true);

    ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
}