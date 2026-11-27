#include "AbilitySystem/Abilities/TBGameplayAbility_Projectile.h"

#include "TBGameplayTags.h"
#include "Character/TBCharacterBase.h"
#include "Engine/HitResult.h"
#include "Projectiles/TBProjectile.h"

UTBGameplayAbility_Projectile::UTBGameplayAbility_Projectile()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerExecution;
    ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateNo;

    // 발사 동작과 후딜 동안 후속 공격 제한
    ActivationOwnedTags.AddTag(TBGameplayTags::State_Combat);
}

bool UTBGameplayAbility_Projectile::GetProjectileLaunchTransform_Implementation(FTransform& OutTransform) const
{
    const ATBCharacterBase* Attacker = Cast<ATBCharacterBase>(GetAvatarActorFromActorInfo());

    if (!IsValid(Attacker) || !Attacker->GetWorld())
    {
        return false;
    }

    if (!FMath::IsFinite(ProjectileAimTraceDistance) || ProjectileAimTraceDistance <= 0.0f || !FMath::IsFinite(ProjectileSpawnHeightOffset))
    {
        return false;
    }

    // 서버 캐릭터의 눈 위치와 조준 회전
    FVector EyeLocation;
    FRotator ViewRotation;
    Attacker->GetActorEyesViewPoint(EyeLocation, ViewRotation);

    if (EyeLocation.ContainsNaN() || ViewRotation.ContainsNaN())
    {
        return false;
    }

    // 조준선에 충돌이 없을 때 사용할 끝점
    const FVector TraceEnd = EyeLocation + ViewRotation.Vector() * ProjectileAimTraceDistance;

    // 실제 투사체와 동일한 Projectile 응답을 기준으로 조준 검사
    constexpr ECollisionChannel ProjectileChannel = ECC_GameTraceChannel4;

    FHitResult AimHit;
    const bool bBlockingHit = Attacker->LineTraceByChannel(AimHit, EyeLocation, TraceEnd, ProjectileChannel);

    // 첫 차단 지점 또는 조준 검사 거리의 끝점
    const FVector AimPoint = bBlockingHit ? FVector(AimHit.ImpactPoint) : TraceEnd;

    // 시선의 상하 각도와 관계없이 월드 Z축으로 발사 높이 조절
    const FVector SpawnLocation = EyeLocation + FVector::UpVector * ProjectileSpawnHeightOffset;

    // 실제 발사 위치에서 조준 지점을 향하는 방향
    FVector LaunchDirection = AimPoint - SpawnLocation;

    if (SpawnLocation.ContainsNaN() || LaunchDirection.ContainsNaN() || !LaunchDirection.Normalize())
    {
        return false;
    }

    OutTransform = FTransform(LaunchDirection.Rotation(), SpawnLocation);
    return OutTransform.IsValid();
}

void UTBGameplayAbility_Projectile::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    ATBCharacterBase* Attacker = ActorInfo ? Cast<ATBCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;

    if (!IsValid(Attacker) || !ProjectileClass || (bWaitForRecovery && !ThirdPersonMontage))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 피해량 확정, 비용·쿨다운 처리, 몽타주 재생
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    if (!IsActive())
    {
        return;
    }

    if (ActorInfo->IsNetAuthority())
    {
        FTransform SpawnTransform;

        if (!GetProjectileLaunchTransform(SpawnTransform))
        {
            EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
            return;
        }

        if (!IsActive())
        {
            return;
        }

        ATBProjectile* Projectile = Attacker->SpawnAttackProjectile(ProjectileClass, SpawnTransform);

        if (!IsValid(Projectile))
        {
            EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
            return;
        }

        if (!IsActive())
        {
            Projectile->Destroy();
            return;
        }

        ActiveProjectile = Projectile;

        // 발사 전에 확정 피해량과 명중 조건 전달
        if (!Projectile->InitializeAttackDamage(Attacker, CurrentAttackDamage, AllowedTargetClasses, RequiresUnblockedAttackerOnHit()))
        {
            EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
            return;
        }

        const bool bLaunchSucceeded = Projectile->Launch();

        // 발사 중 외부 처리로 어빌리티가 종료된 경우 확인
        if (!IsActive())
        {
            return;
        }

        if (!bLaunchSucceeded)
        {
            EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
            return;
        }

        MarkAttackActionCompleted();

        // 발사 성공 이후에는 투사체가 자신의 수명 관리
        ActiveProjectile.Reset();
    }

    // 후딜 대기 여부에 따른 발사 동작 종료
    if (!bWaitForRecovery)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UTBGameplayAbility_Projectile::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (!IsActive())
    {
        return;
    }

    ATBProjectile* Projectile = ActiveProjectile.Get();
    ActiveProjectile.Reset();

    // 발사 준비 중 실패한 서버 투사체 정리
    if (IsValid(Projectile) && Projectile->HasAuthority() && !Projectile->HasLaunched() && !Projectile->IsActorBeingDestroyed())
    {
        Projectile->Destroy();
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
