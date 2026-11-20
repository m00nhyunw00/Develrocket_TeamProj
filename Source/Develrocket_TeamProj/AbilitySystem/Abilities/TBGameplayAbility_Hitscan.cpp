#include "TBGameplayAbility_Hitscan.h"

#include "AbilitySystem/TBAbilitySystemComponent.h"
#include "Character/Player/TBPlayer.h"
#include "Framework/TBPlayerState.h"
#include "GameplayEffect.h"
#include "TBGameplayTags.h"

UTBGameplayAbility_Hitscan::UTBGameplayAbility_Hitscan()
{
    // 기존 기본 공격의 피해량 조회 기본값; 기능별 BP에서 변경 가능
    AttackId = TBGameplayTags::Ability_Combat_PrimaryAttack;

    ActivationOwnedTags.AddTag(TBGameplayTags::State_Combat);
    
    BulletTrailCueTag = TBGameplayTags::GameplayCue_Weapon_BloodTrail;
    DefaultImpactCueTag = TBGameplayTags::GameplayCue_Impact_Blood_World;

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
    if (!IsAllowedAttackHit(HitResult))
    {
        return;
    }

    ATBCharacterBase* Attacker = Cast<ATBCharacterBase>(GetAvatarActorFromActorInfo());
    Attacker->ApplyHitscanDamage(HitResult, CurrentAttackData);
}
