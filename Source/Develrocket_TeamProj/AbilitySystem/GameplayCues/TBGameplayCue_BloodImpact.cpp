#include "TBGameplayCue_BloodImpact.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/DecalComponent.h"
#include "Kismet/GameplayStatics.h"

UTBGameplayCue_BloodImpact::UTBGameplayCue_BloodImpact()
{
    // 상위 Gameplay Cue 태그에 등록된 연출의 중복 실행 방지
    IsOverride = true;
}

bool UTBGameplayCue_BloodImpact::HandlesEvent(EGameplayCueEvent::Type EventType) const
{
    // 순간 피격 연출에 해당하는 실행 이벤트만 처리
    return EventType == EGameplayCueEvent::Executed;
}

bool UTBGameplayCue_BloodImpact::OnExecute_Implementation(
    AActor* MyTarget,
    const FGameplayCueParameters& Parameters) const
{
    // 전용 서버에서는 시각 컴포넌트 생성 제외
    if (!IsValid(MyTarget) || !MyTarget->GetWorld()
        || MyTarget->GetNetMode() == NM_DedicatedServer)
    {
        return false;
    }

    // 서버에서 전달한 충돌 위치와 표면 방향
    const FVector ImpactLocation = Parameters.Location;
    const FVector ImpactNormal = FVector(Parameters.Normal).GetSafeNormal();

    if (ImpactNormal.IsNearlyZero())
    {
        return false;
    }

    const FVector SpawnLocation = ImpactLocation + ImpactNormal * SurfaceOffset;

    // Niagara 생성 후 파라미터를 지정하고 재생
    const auto SpawnParticle = [&](UNiagaraSystem* System, const FLinearColor& Color)
    {
        if (!IsValid(System))
        {
            return false;
        }

        UNiagaraComponent* Particle = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            MyTarget, System, SpawnLocation, ImpactNormal.Rotation(),
            FVector::OneVector, true, false, ENCPoolMethod::None, false);

        if (!IsValid(Particle))
        {
            return false;
        }

        Particle->SetVariableLinearColor(TEXT("User.BloodColor"), Color);
        Particle->SetVariableVec3(TEXT("User.ImpactNormal"), ImpactNormal);
        Particle->Activate(true);

        return true;
    };

    // 혈탄 자체의 색상은 대상과 관계없이 붉은색 유지
    const FLinearColor BulletBloodColor(0.4f, 0.01f, 0.01f, 1.0f);
    const bool bBulletSpawned = SpawnParticle(BulletImpactSystem.Get(), BulletBloodColor);

    // 대상 혈액 파티클은 별도 생성하며, 미지정 시 생략
    const bool bTargetSpawned = SpawnParticle(TargetBloodSystem.Get(), TargetBloodColor);
    bool bSpawned = bBulletSpawned || bTargetSpawned;

    if (!IsValid(DecalMaterial.Get()))
    {
        return bSpawned;
    }

    // 데칼의 투영 방향을 충돌 표면 안쪽으로 정렬
    const FRotator DecalRotation = (-ImpactNormal).Rotation();
    const float HoldTime = FMath::Max(0.0f, DecalHoldTime);
    const float FadeTime = FMath::Max(0.01f, DecalFadeDuration);
    UDecalComponent* Decal = nullptr;

    if (bAttachDecalToHitBone)
    {
        // 실제 피격 메시와 본은 공격자의 MyTarget이 아닌 HitResult에서 확인
        const FHitResult* HitResult = Parameters.EffectContext.GetHitResult();
        USkeletalMeshComponent* HitMesh = HitResult
            ? Cast<USkeletalMeshComponent>(HitResult->GetComponent())
            : nullptr;

        // 부착할 메시와 본이 없으면 생명체 데칼 생성 생략
        if (!IsValid(HitMesh) || HitResult->BoneName.IsNone()
            || HitMesh->GetBoneIndex(HitResult->BoneName) == INDEX_NONE)
        {
            return bSpawned;
        }

        // 생명체의 움직임을 따라가도록 충돌한 본에 부착
        Decal = UGameplayStatics::SpawnDecalAttached(
            DecalMaterial.Get(), DecalSize, HitMesh, HitResult->BoneName,
            SpawnLocation, DecalRotation, EAttachLocation::KeepWorldPosition,
            HoldTime + FadeTime);
    }
    else
    {
        // 벽·바닥의 충돌 위치에 환경 데칼 생성
        Decal = UGameplayStatics::SpawnDecalAtLocation(MyTarget, DecalMaterial.Get(), DecalSize, SpawnLocation, DecalRotation, HoldTime + FadeTime);
    }

    if (IsValid(Decal))
    {
        // 풀 재사용 시 피격 데칼만 식별할 수 있도록 표시
        Decal->ComponentTags.AddUnique(FName(TEXT("TB.HitImpactDecal")));

        // 선택된 대상의 혈액 색상을 데칼에 적용
        if (UMaterialInstanceDynamic* Material = Decal->CreateDynamicMaterialInstance())
        {
            Material->SetVectorParameterValue(TEXT("BloodColor"), TargetBloodColor);
        }

        // 페이드 종료 시 데칼 소유 액터의 삭제 방지
        Decal->SetFadeOut(HoldTime, FadeTime, false);
        bSpawned = true;
    }

    return bSpawned;
}