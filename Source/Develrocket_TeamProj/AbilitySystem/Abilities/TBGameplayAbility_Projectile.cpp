#include "AbilitySystem/Abilities/TBGameplayAbility_Projectile.h"

#include "Character/TBCharacterBase.h"
#include "Projectiles/TBProjectile.h"

UTBGameplayAbility_Projectile::UTBGameplayAbility_Projectile()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerExecution;

    // 실행 인스턴스 자체는 복제하지 않고 GAS 실행·종료 통신 사용
    ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateNo;
}

bool UTBGameplayAbility_Projectile::GetProjectileLaunchTransform_Implementation(FTransform& OutTransform) const
{
    const ATBCharacterBase* Attacker = Cast<ATBCharacterBase>(GetAvatarActorFromActorInfo());

    if (!IsValid(Attacker))
    {
        return false;
    }

    FVector Location;
    FRotator Rotation;
    Attacker->GetActorEyesViewPoint(Location, Rotation);

    if (Location.ContainsNaN() || Rotation.ContainsNaN())
    {
        return false;
    }

    OutTransform = FTransform(Rotation, Location);
    return true;
}

void UTBGameplayAbility_Projectile::ActivateAbility(
    FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    ATBCharacterBase* Attacker = ActorInfo
        ? Cast<ATBCharacterBase>(ActorInfo->AvatarActor.Get())
        : nullptr;

    if (!IsValid(Attacker) || !ProjectileClass)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    DamagedActors.Reset();

    // 피해량 확정, 비용·쿨다운 처리, 선택적 몽타주 재생
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    if (!IsActive() || !ActorInfo->IsNetAuthority())
    {
        return;
    }

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

    HitDelegateHandle = Projectile->OnHit.AddUObject(this, &ThisClass::HandleProjectileHit);

    FinishedDelegateHandle = Projectile->OnFinished.AddUObject(this, &ThisClass::HandleProjectileFinished);

    const bool bLaunchSucceeded = Projectile->Launch();

    // Launch 중 즉시 명중·종료된 경우 콜백에서 이미 정리
    if (!IsActive())
    {
        return;
    }

    if (!bLaunchSucceeded)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 발사 행동은 완료하되 명중을 받을 어빌리티는 유지
    MarkAttackActionCompleted();
}

void UTBGameplayAbility_Projectile::HandleProjectileHit(const FHitResult& HitResult)
{
    if (!IsAllowedAttackHit(HitResult))
    {
        return;
    }

    ATBCharacterBase* Attacker = Cast<ATBCharacterBase>(GetAvatarActorFromActorInfo());

    AActor* Target = HitResult.GetActor();
    const TWeakObjectPtr<AActor> TargetKey(Target);

    if (DamagedActors.Contains(TargetKey))
    {
        return;
    }

    // 피해 처리 중 재진입하더라도 중복 적용되지 않도록 먼저 기록
    DamagedActors.Add(TargetKey);

    if (!Attacker->ApplyProjectileDamage(HitResult, CurrentAttackDamage))
    {
        // 피해 요청이 실패한 대상은 성공 기록에서 제외
        DamagedActors.Remove(TargetKey);
    }
}

void UTBGameplayAbility_Projectile::HandleProjectileFinished(bool bWasLaunched)
{
    if (!IsActive())
    {
        return;
    }

    // 발사 직후 즉시 종료된 경우도 발사 완료로 기록
    if (bWasLaunched)
    {
        MarkAttackActionCompleted();
    }

    EndAbility(
        CurrentSpecHandle,
        CurrentActorInfo,
        CurrentActivationInfo,
        true,
        !bWasLaunched);
}

void UTBGameplayAbility_Projectile::EndAbility(
    FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility,
    bool bWasCancelled)
{
    if (!IsActive())
    {
        return;
    }

    ATBProjectile* Projectile = ActiveProjectile.Get();
    ActiveProjectile.Reset();

    if (IsValid(Projectile))
    {
        Projectile->OnHit.Remove(HitDelegateHandle);
        Projectile->OnFinished.Remove(FinishedDelegateHandle);
    }

    HitDelegateHandle.Reset();
    FinishedDelegateHandle.Reset();
    DamagedActors.Reset();

    // 어빌리티가 먼저 종료되면 서버 투사체도 정리
    if (IsValid(Projectile) && Projectile->HasAuthority() && !Projectile->IsActorBeingDestroyed())
    {
        Projectile->Destroy();
    }

    Super::EndAbility(
        Handle, ActorInfo, ActivationInfo,
        bReplicateEndAbility, bWasCancelled);
}