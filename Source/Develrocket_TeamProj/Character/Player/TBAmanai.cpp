#include "Character/Player/TBAmanai.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/TBGameplayAbility_DashAttack.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

ATBAmanai::ATBAmanai(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void ATBAmanai::BeginMeleeTrace(USkeletalMeshComponent* SourceMesh)
{
    // 기존 서버 검 판정을 먼저 열고 첫 소켓 위치 기록
    Super::BeginMeleeTrace(SourceMesh);

    if (SourceMesh != GetMesh() || IsActionBlocked() || (!HasAuthority() && !IsLocallyControlled()))
    {
        return;
    }

    UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
    UTBGameplayAbility_DashAttack* Dash = ASC
        ? Cast<UTBGameplayAbility_DashAttack>(ASC->GetAnimatingAbility()) : nullptr;

    if (Dash)
    {
        Dash->StartDashFromMeleeTrace(*this);
    }
}

void ATBAmanai::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // 소유 클라이언트는 예측 실행에서 직접 적용
    DOREPLIFETIME_CONDITION(ATBAmanai, bDashPawnPassThrough, COND_SimulatedOnly);
}

void ATBAmanai::SetDashPawnPassThrough(bool bEnabled)
{
    if (bDashPawnPassThrough == bEnabled)
    {
        return;
    }

    bDashPawnPassThrough = bEnabled;
    OnRep_DashPawnPassThrough();

    if (HasAuthority())
    {
        ForceNetUpdate();
    }
}

void ATBAmanai::OnRep_DashPawnPassThrough()
{
    UCapsuleComponent* Capsule = GetCapsuleComponent();

    if (bDashPawnPassThrough)
    {
        if (!bHasSavedPawnResponse)
        {
            SavedPawnResponse = Capsule->GetCollisionResponseToChannel(ECC_Pawn);
            bHasSavedPawnResponse = true;
        }

        Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
        return;
    }

    TryRestoreDashPawnCollision();
}

void ATBAmanai::TryRestoreDashPawnCollision()
{
    if (!bHasSavedPawnResponse || bDashPawnPassThrough)
    {
        return;
    }

    UCapsuleComponent* Capsule = GetCapsuleComponent();

    if (SavedPawnResponse == ECR_Block)
    {
        // 기존 설정으로 복구했을 때 실제로 막힐 Pawn과 겹쳐 있는지 확인
        FCollisionQueryParams Query(SCENE_QUERY_STAT(AmanaiDashExit), false, this);
        FCollisionResponseParams PawnOnly(ECR_Ignore);
        PawnOnly.CollisionResponse.SetResponse(ECC_Pawn, ECR_Block);

        if (GetWorld()->OverlapBlockingTestByChannel(Capsule->GetComponentLocation(),
            Capsule->GetComponentQuat(), Capsule->GetCollisionObjectType(),
            Capsule->GetCollisionShape(), Query, PawnOnly))
        {
            return;
        }
    }

    bHasSavedPawnResponse = false;
    Capsule->SetCollisionResponseToChannel(ECC_Pawn, SavedPawnResponse);
}

void ATBAmanai::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // 도착 시 다른 Pawn과 겹쳤던 경우에만 분리 시점까지 재확인
    if (!bDashPawnPassThrough && bHasSavedPawnResponse)
    {
        TryRestoreDashPawnCollision();
    }
}