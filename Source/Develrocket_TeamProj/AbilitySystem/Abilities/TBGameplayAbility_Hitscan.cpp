#include "AbilitySystem/Abilities/TBGameplayAbility_Hitscan.h"

#include "AbilitySystem/TBAbilitySystemComponent.h"
#include "Character/Player/TBPlayer.h"
#include "Framework/TBPlayerState.h"
#include "TBGameplayTags.h"
#include "Perception/AISense_Hearing.h"

UTBGameplayAbility_Hitscan::UTBGameplayAbility_Hitscan()
{
    // 기존 기본 공격의 피해량 조회 기본값; BP에서 변경 가능
    AttackId = TBGameplayTags::Ability_Combat_PrimaryAttack;

    ActivationOwnedTags.AddTag(TBGameplayTags::State_Combat);

    AttackVisualCueTag = TBGameplayTags::GameplayCue_Attack_Weapon_BloodTrail;
    DefaultImpactCueTag = TBGameplayTags::GameplayCue_Impact_Blood_World;
}

void UTBGameplayAbility_Hitscan::ActivateAbility(
    FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    // 후딜 구간 판정에 사용할 3인칭 몽타주 확인
    if (bWaitForRecovery && !ThirdPersonMontage)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    
    ATBPlayer* Player = ActorInfo
        ? Cast<ATBPlayer>(ActorInfo->AvatarActor.Get())
        : nullptr;

    if (!IsValid(Player))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    float TraceRange = HitscanRange;
    bool bDrawTrace = bDrawHitscanTrace;

    const bool bPrimaryAttack =
        AttackId == TBGameplayTags::Ability_Combat_PrimaryAttack;

    if (bPrimaryAttack)
    {
        const ATBPlayerState* PS =
            Cast<ATBPlayerState>(ActorInfo->OwnerActor.Get());

        if (!IsValid(PS) || !GetCostGameplayEffect())
        {
            EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
            return;
        }

        TraceRange = PS->GetPrimaryAttackRange();
        bDrawTrace = PS->ShouldDrawPrimaryAttackTrace();
    }

    if (!FMath::IsFinite(TraceRange) || TraceRange <= 0.0f)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    Super::ActivateAbility(
        Handle, ActorInfo, ActivationInfo, TriggerEventData);

    if (!IsActive())
    {
        return;
    }

    if (ActorInfo->IsNetAuthority())
    {
        // 총소리를 AI 청각에 알림
        UAISense_Hearing::ReportNoiseEvent(Player->GetWorld(), Player->GetActorLocation(), 1.f, Player, 0.f, FName(TEXT("Gunshot")));

        const FHitResult HitResult = Player->TraceHitscanShot(AttackVisualCueTag, DefaultImpactCueTag, TraceRange, bDrawTrace, bCurrentAttackEnhanced);

        if (HitResult.bBlockingHit)
        {
            HandleHitscanHit(HitResult);
        }
    }

    if (Player->IsLocallyControlled())
    {
        Player->StartCameraRecoil(
            CameraRecoilPitch, CameraRecoilRiseTime, CameraRecoilReturnTime);

        if (bPrimaryAttack)
        {
            if (UTBAbilitySystemComponent* TBASC =
                Cast<UTBAbilitySystemComponent>(
                    ActorInfo->AbilitySystemComponent.Get()))
            {
                TBASC->OnPrimaryShotFired.Broadcast();
            }
        }
    }

    // 기존 발사·명중·반동 처리 이후 종료 시점 선택
    if (!bWaitForRecovery)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }}

void UTBGameplayAbility_Hitscan::HandleHitscanHit(const FHitResult& HitResult)
{
    if (!IsAllowedAttackHit(HitResult))
    {
        return;
    }

    ATBCharacterBase* Attacker =
        Cast<ATBCharacterBase>(GetAvatarActorFromActorInfo());

    Attacker->ApplyHitscanDamage(HitResult, CurrentAttackDamage);
}
