#include "AbilitySystem/GameplayCues/TBGameplayCue_BloodBulletTrail.h"

#include "Character/Player/TBPlayer.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

#include "AbilitySystem/GameplayCues/TBGameplayCue_BloodBulletTrail.h"

#include "Character/Player/TBPlayer.h"
#include "Components/SkeletalMeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

bool UTBGameplayCue_BloodBulletTrail::OnExecute_Implementation(
    AActor* MyTarget,
    const FGameplayCueParameters& Parameters) const
{
    ATBPlayer* Shooter = Cast<ATBPlayer>(Parameters.EffectCauser.Get());

    if (!IsValid(Shooter)
        || Shooter->GetNetMode() == NM_DedicatedServer
        || !IsValid(TrailSystem.Get()))
    {
        return false;
    }

    // 발사 시점의 총구 위치
    FVector TrailStart;

    if (!Shooter->TryGetWeaponSocketLocation(MuzzleSocketName, TrailStart))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("%s: 총구 소켓 %s를 찾지 못했습니다."),
            *GetNameSafe(Shooter),
            *MuzzleSocketName.ToString());

        return false;
    }

    // 서버가 결정한 충돌 지점 또는 최대 사거리 끝점
    const FVector TrailEnd = Parameters.Location;
    const FVector TrailDirection = TrailEnd - TrailStart;

    if (TrailDirection.IsNearlyZero())
    {
        return false;
    }

    UNiagaraComponent* TrailComponent = nullptr;

    if (Shooter->IsLocallyControlled())
    {
        // 본인 화면: 1인칭 총구 소켓에 부착하여 이동 추적
        USkeletalMeshComponent* FirstPersonMesh =
            Shooter->GetFirstPersonMesh();

        if (!IsValid(FirstPersonMesh))
        {
            return false;
        }

        TrailComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
            TrailSystem.Get(),
            FirstPersonMesh,
            MuzzleSocketName,
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            EAttachLocation::SnapToTarget,
            true,
            false,
            ENCPoolMethod::None,
            false);
    }
    else
    {
        // 다른 플레이어 화면: 발사 시점의 월드 위치 유지
        TrailComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            Shooter,
            TrailSystem.Get(),
            TrailStart,
            TrailDirection.Rotation(),
            FVector::OneVector,
            true,
            false,
            ENCPoolMethod::None,
            false);
    }

    if (!IsValid(TrailComponent))
    {
        return false;
    }

    // 끝점은 발사 당시 서버 판정 위치로 고정
    TrailComponent->SetVariablePosition(TEXT("User.TrailEnd"), TrailEnd);

    TrailComponent->Activate(true);
    return true;
}