#include "TBGameplayAbility_Hitscan.h"

#include "AbilitySystemGlobals.h"
#include "AbilitySystem/TBAbilitySystemComponent.h"
#include "Character/Player/TBPlayer.h"
#include "Framework/TBPlayerState.h"
#include "GameplayEffect.h"
#include "TBGameplayTags.h"
#include "Character/Human/TBCitizen.h"
#include "Character/Monster/TBMonster.h"

UTBGameplayAbility_Hitscan::UTBGameplayAbility_Hitscan()
{
    // SetAssetTags(FGameplayTagContainer(TBGameplayTags::Ability_Combat_PrimaryAttack));

    // 소유 클라이언트 예측 실행 및 서버 검증
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // 발사 시작 시 비용 및 쿨다운 적용
    bCommitOnActivate = true;

    // 일반 공격의 상태 및 실행 제한
    ActivationOwnedTags.AddTag(TBGameplayTags::State_Combat);

    ActivationBlockedTags.AddTag(TBGameplayTags::State_Combat);
    ActivationBlockedTags.AddTag(TBGameplayTags::State_Dead);
    ActivationBlockedTags.AddTag(TBGameplayTags::State_Stunned);
    ActivationBlockedTags.AddTag(TBGameplayTags::State_Draining);
    
    BulletTrailCueTag = TBGameplayTags::GameplayCue_Weapon_BloodTrail;
    DefaultImpactCueTag = TBGameplayTags::GameplayCue_Impact_Blood_World;

    DamageTypeTag = TBGameplayTags::Damage_Type_Bullet;
    AllowedTargetClasses = {
        ATBPlayer::StaticClass(),
        ATBMonster::StaticClass(),
        ATBCitizen::StaticClass()
    };
}

void UTBGameplayAbility_Hitscan::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    ATBPlayer* Player = ActorInfo ? Cast<ATBPlayer>(ActorInfo->AvatarActor.Get()) : nullptr;

    const ATBPlayerState* PS = ActorInfo ? Cast<ATBPlayerState>(ActorInfo->OwnerActor.Get()) : nullptr;

    // 일반 공격의 필수 대상 및 비용 설정 검증
    if (!IsValid(Player) || !IsValid(PS) || !GetCostGameplayEffect() || !bCommitOnActivate)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    const float Interval = PS->GetPrimaryAttackInterval();
    const TSubclassOf<UGameplayEffect> CooldownEffectClass = PS->GetPrimaryAttackCooldownEffectClass();

    const UGameplayEffect* CooldownEffect = CooldownEffectClass ? CooldownEffectClass.GetDefaultObject() : nullptr;

    // 잘못된 설정으로 발사 간격 제한이 사라지는 상황 방지
    if (!FMath::IsFinite(Interval) || Interval <= 0.0f
        || !CooldownEffect
        || CooldownEffect->DurationPolicy != EGameplayEffectDurationType::HasDuration
        || !CooldownEffect->GetGrantedTags().HasTagExact(TBGameplayTags::Cooldown_PrimaryAttack))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 탄약·쿨다운 처리 및 몽타주 재생
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
    if (!IsActive())
    {
        return;
    }

    if (ActorInfo->IsNetAuthority())
    {
        const FHitResult HitResult = Player->TraceHitscanShot(BulletTrailCueTag, DefaultImpactCueTag);

        if (HitResult.bBlockingHit)
        {
            HandleHitscanHit(HitResult);
        }
    }

    // 소유 플레이어 화면의 발사 반동 및 알림
    if (Player->IsLocallyControlled())
    {
        Player->StartCameraRecoil(CameraRecoilPitch,CameraRecoilRiseTime,CameraRecoilReturnTime);

        if (UTBAbilitySystemComponent* TBASC = Cast<UTBAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()))
        {
            TBASC->OnPrimaryShotFired.Broadcast();
        }
    }

    // 한 발 처리 완료
    // 몽타주는 유지하고 다음 발사는 쿨다운으로 제한
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

const FGameplayTagContainer* UTBGameplayAbility_Hitscan::GetCooldownTags() const
{
    static const FGameplayTagContainer CooldownTags(TBGameplayTags::Cooldown_PrimaryAttack);

    return &CooldownTags;
}

void UTBGameplayAbility_Hitscan::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
    const ATBPlayerState* PS = ActorInfo ? Cast<ATBPlayerState>(ActorInfo->OwnerActor.Get()) : nullptr;

    if (!IsValid(PS))
    {
        return;
    }

    const float Interval = PS->GetPrimaryAttackInterval();
    const TSubclassOf<UGameplayEffect> CooldownEffectClass = PS->GetPrimaryAttackCooldownEffectClass();

    if (!CooldownEffectClass || !FMath::IsFinite(Interval) || Interval <= 0.0f)
    {
        return;
    }

    FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo, CooldownEffectClass, GetAbilityLevel(Handle, ActorInfo));

    if (!Spec.IsValid())
    {
        return;
    }

    // 이번 발사의 쿨다운 시간을 PlayerState 설정으로 지정
    Spec.Data->SetDuration(Interval, true);

    // 소유 클라이언트 예측 및 서버 적용
    ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
}

void UTBGameplayAbility_Hitscan::HandleHitscanHit(const FHitResult& HitResult)
{
    ATBCharacterBase* Attacker = Cast<ATBCharacterBase>(GetAvatarActorFromActorInfo());
    AActor* Target = HitResult.GetActor();

    if (!IsActive() || !IsValid(Attacker) || !Attacker->HasAuthority()
        || Attacker->IsActionBlocked() || !IsValid(Target) || Target == Attacker
        || !Target->Implements<UTBHitInterface>())
    {
        return;
    }

    const bool bAllowedTarget = AllowedTargetClasses.ContainsByPredicate(
        [Target](const TSubclassOf<AActor>& TargetClass)
        {
            return TargetClass.Get() && Target->IsA(TargetClass.Get());
        });

    if (!bAllowedTarget)
    {
        return;
    }

    UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);

    if (!IsValid(TargetASC) || TargetASC->GetAvatarActor() != Target
        || TargetASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead)
        || TargetASC->HasMatchingGameplayTag(TBGameplayTags::State_Invulnerable))
    {
        return;
    }

    Attacker->ApplyHitscanDamage(HitResult, DamageTypeTag);
}
