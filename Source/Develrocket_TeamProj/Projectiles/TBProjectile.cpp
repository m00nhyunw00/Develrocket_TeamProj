#include "Projectiles/TBProjectile.h"

#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"

ATBProjectile::ATBProjectile()
{
    PrimaryActorTick.bCanEverTick = false;

    bReplicates = true;
    SetReplicateMovement(true);

    ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));

    SetRootComponent(ProjectileMesh);

    ProjectileMesh->SetMobility(EComponentMobility::Movable);
    ProjectileMesh->SetCollisionProfileName(TEXT("TBProjectile"));
    ProjectileMesh->SetGenerateOverlapEvents(false);
    ProjectileMesh->SetSimulatePhysics(false);

    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));

    ProjectileMovement->SetUpdatedComponent(ProjectileMesh);

    // 어빌리티 구독 완료 전에는 이동하지 않음
    ProjectileMovement->bAutoActivate = false;
    ProjectileMovement->PrimaryComponentTick.bStartWithTickEnabled = false;

    ProjectileMovement->bSweepCollision = true;
    ProjectileMovement->bShouldBounce = false;
    ProjectileMovement->bRotationFollowsVelocity = true;

    // 이동 중 겹침 통지를 차단 충돌 뒤로 지연하지 않음
    ProjectileMovement->bSimulationUseScopedMovement = false;

    ProjectileMovement->InitialSpeed = 1800.0f;
    ProjectileMovement->MaxSpeed = 1800.0f;
    ProjectileMovement->ProjectileGravityScale = 0.0f;
}

void ATBProjectile::BeginPlay()
{
    Super::BeginPlay();

    // Launch 전에는 충돌 판정도 비활성화
    ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    if (!HasAuthority())
    {
        // 클라이언트는 서버에서 복제된 위치로 외형 표시
        ProjectileMovement->Deactivate();
        ProjectileMovement->SetComponentTickEnabled(false);
        return;
    }

    ProjectileMesh->OnComponentHit.AddDynamic(this, &ThisClass::HandleCollision);

    ProjectileMesh->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleOverlap);

    // 캐릭터 관통 여부에 따라 Pawn 응답만 변경
    ProjectileMesh->SetCollisionResponseToChannel(ECC_Pawn, bPiercing ? ECR_Overlap : ECR_Block);

    ProjectileMesh->SetGenerateOverlapEvents(bPiercing);

    if (AActor* Source = GetOwner())
    {
        // 투사체가 이동할 때 발사자 제외
        ProjectileMesh->IgnoreActorWhenMoving(Source, true);

        if (UPrimitiveComponent* SourceRoot = Cast<UPrimitiveComponent>(Source->GetRootComponent()))
        {
            // 발사자가 이동할 때도 자신의 투사체 제외
            SourceRoot->IgnoreActorWhenMoving(this, true);
            IgnoredSourceRoot = SourceRoot;
        }
    }

    SetLifeSpan(FMath::Max(LifeSeconds, 0.1f));
}

bool ATBProjectile::Launch()
{
    if (!HasAuthority() || bLaunched || bResolved
        || IsActorBeingDestroyed()
        || !ProjectileMesh->GetStaticMesh()
        || !FMath::IsFinite(ProjectileMovement->InitialSpeed)
        || ProjectileMovement->InitialSpeed <= 0.0f)
    {
        return false;
    }

    bLaunched = true;

    ProjectileMovement->Velocity = GetActorForwardVector() * ProjectileMovement->InitialSpeed;

    ProjectileMovement->Activate(true);

    // 즉시 겹침 콜백이 발생할 수 있으므로 발사 상태부터 기록
    ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

    if (!IsActorBeingDestroyed())
    {
        ForceNetUpdate();
    }

    // 즉시 명중하여 종료되더라도 발사 자체는 성공
    return true;
}

void ATBProjectile::HandleCollision(
    UPrimitiveComponent* HitComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComponent,
    FVector NormalImpulse,
    const FHitResult& HitResult)
{
    if (!HasAuthority() || !bLaunched || bResolved
        || OtherActor == GetOwner()
        || OtherActor == GetInstigator())
    {
        return;
    }

    // 차단 충돌은 한 번만 처리
    bResolved = true;

    ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ProjectileMovement->StopMovementImmediately();
    ProjectileMovement->Deactivate();

    OnHit.Broadcast(HitResult);

    if (!IsActorBeingDestroyed())
    {
        Destroy();
    }
}

void ATBProjectile::HandleOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComponent,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (!HasAuthority() || !bLaunched || bResolved || !bPiercing
        || !IsValid(OtherActor) || !IsValid(OtherComponent)
        || OtherActor == this
        || OtherActor == GetOwner()
        || OtherActor == GetInstigator()
        || OtherComponent->GetCollisionObjectType() != ECC_Pawn)
    {
        return;
    }

    FHitResult HitResult = SweepResult;

    // 처음부터 겹친 경우처럼 유효한 Sweep 정보가 없는 상황 보완
    if (!bFromSweep || HitResult.GetActor() != OtherActor || HitResult.GetComponent() != OtherComponent)
    {
        HitResult = FHitResult(
            OtherActor,
            OtherComponent,
            OtherComponent->GetComponentLocation(),
            FVector::ZeroVector);

        HitResult.Item = OtherBodyIndex;
    }

    // 관통 대상은 통지만 수행하고 이동 유지
    OnHit.Broadcast(HitResult);
}

void ATBProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UPrimitiveComponent* SourceRoot = IgnoredSourceRoot.Get())
    {
        SourceRoot->IgnoreActorWhenMoving(this, false);
    }

    IgnoredSourceRoot.Reset();

    if (HasAuthority())
    {
        OnFinished.Broadcast(bLaunched);
    }

    Super::EndPlay(EndPlayReason);
}