#include "Projectiles/TBProjectile.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/TBGameplayAbility_Attack.h"
#include "Character/TBCharacterBase.h"
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

    // 발사 준비 완료 전 이동 비활성화
    ProjectileMovement->bAutoActivate = false;
    ProjectileMovement->PrimaryComponentTick.bStartWithTickEnabled = false;

    ProjectileMovement->bSweepCollision = true;
    ProjectileMovement->bShouldBounce = false;
    ProjectileMovement->bRotationFollowsVelocity = true;

    // 이동 중 겹침 통지의 지연 방지
    ProjectileMovement->bSimulationUseScopedMovement = false;

    ProjectileMovement->InitialSpeed = 1800.0f;
    ProjectileMovement->MaxSpeed = 1800.0f;
    ProjectileMovement->ProjectileGravityScale = 0.0f;
}

void ATBProjectile::BeginPlay()
{
    Super::BeginPlay();

    // 발사 전 충돌 판정 비활성화
    ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    if (!HasAuthority())
    {
        // 서버에서 복제된 위치로 클라이언트 외형 표시
        ProjectileMovement->Deactivate();
        ProjectileMovement->SetComponentTickEnabled(false);
        return;
    }

    ProjectileMesh->OnComponentHit.AddDynamic(this, &ThisClass::HandleCollision);

    ProjectileMesh->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleOverlap);

    // 관통 여부에 따른 Pawn 충돌 응답 설정
    ProjectileMesh->SetCollisionResponseToChannel(ECC_Pawn, bPiercing ? ECR_Overlap : ECR_Block);

    ProjectileMesh->SetGenerateOverlapEvents(bPiercing);

    if (AActor* Source = GetOwner())
    {
        // 투사체 이동 시 발사자 충돌 제외
        ProjectileMesh->IgnoreActorWhenMoving(Source, true);

        if (UPrimitiveComponent* SourceRoot = Cast<UPrimitiveComponent>(Source->GetRootComponent()))
        {
            // 발사자 이동 시 자신의 투사체 충돌 제외
            SourceRoot->IgnoreActorWhenMoving(this, true);
            IgnoredSourceRoot = SourceRoot;
        }
    }

    SetLifeSpan(FMath::Max(LifeSeconds, 0.1f));
}

bool ATBProjectile::InitializeAttackDamage(ATBCharacterBase* InAttacker, float InDamage, const TArray<TSubclassOf<AActor>>& InAllowedTargetClasses, bool bInRequireUnblockedAttacker)
{
    if (!HasAuthority() || bLaunched || bResolved || !IsValid(InAttacker) || InAttacker != GetOwner() || !FMath::IsFinite(InDamage) || InDamage <= 0.0f || InAllowedTargetClasses.IsEmpty())
    {
        return false;
    }

    UAbilitySystemComponent* SourceASC = InAttacker->GetAbilitySystemComponent();

    if (!IsValid(SourceASC) || SourceASC->GetAvatarActor() != InAttacker)
    {
        return false;
    }

    // 발사 당시 공격 정보 보관
    DamageSource = InAttacker;
    DamageSourceASC = SourceASC;
    DamageAtLaunch = InDamage;
    DamageTargetClasses = InAllowedTargetClasses;
    bRequireUnblockedDamageSource = bInRequireUnblockedAttacker;

    DamagedActors.Reset();

    return true;
}

bool ATBProjectile::Launch()
{
    if (!HasAuthority() || bLaunched || bResolved || IsActorBeingDestroyed() || !ProjectileMesh->GetStaticMesh() || !FMath::IsFinite(ProjectileMovement->InitialSpeed) || ProjectileMovement->InitialSpeed <= 0.0f)
    {
        return false;
    }

    // 즉시 겹침 콜백에 대비한 발사 상태 선행 기록
    bLaunched = true;

    ProjectileMovement->Velocity = GetActorForwardVector() * ProjectileMovement->InitialSpeed;

    ProjectileMovement->Activate(true);

    ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

    if (!IsActorBeingDestroyed())
    {
        ForceNetUpdate();
    }

    // 발사 직후 즉시 명중한 경우도 발사 성공 처리
    return true;
}

void ATBProjectile::ApplyHitDamage(const FHitResult& HitResult)
{
    if (!HasAuthority() || !bLaunched)
    {
        return;
    }

    ATBCharacterBase* Attacker = DamageSource.Get();

    // 발사 당시 정보와 공통 명중 조건 검증
    if (!UTBGameplayAbility_Attack::IsAllowedDamageHit(Attacker, DamageSourceASC.Get(), HitResult, DamageAtLaunch, DamageTargetClasses, bRequireUnblockedDamageSource))
    {
        return;
    }

    const TWeakObjectPtr<AActor> TargetKey(HitResult.GetActor());

    if (DamagedActors.Contains(TargetKey))
    {
        return;
    }

    // 피해 처리 중 재진입에 대비한 선행 기록
    DamagedActors.Add(TargetKey);

    if (!Attacker->ApplyProjectileDamage(HitResult, DamageAtLaunch))
    {
        // 피해 적용에 실패한 대상의 기록 해제
        DamagedActors.Remove(TargetKey);
    }
}

void ATBProjectile::HandleCollision(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& HitResult)
{
    if (!HasAuthority() || !bLaunched || bResolved || OtherActor == GetOwner() || OtherActor == GetInstigator())
    {
        return;
    }

    // 차단 충돌 중복 처리 방지
    bResolved = true;

    ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ProjectileMovement->StopMovementImmediately();
    ProjectileMovement->Deactivate();

    // 투사체 자체 피해 적용 및 명중 통지
    ApplyHitDamage(HitResult);
    OnHit.Broadcast(HitResult);

    // 차단 충돌 완료 후 투사체 종료
    if (!IsActorBeingDestroyed())
    {
        Destroy();
    }
}

void ATBProjectile::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!HasAuthority() || !bLaunched || bResolved || !bPiercing || !IsValid(OtherActor) || !IsValid(OtherComponent) || OtherActor == this || OtherActor == GetOwner() || OtherActor == GetInstigator() || OtherComponent->GetCollisionObjectType() != ECC_Pawn)
    {
        return;
    }

    FHitResult HitResult = SweepResult;

    // 유효한 Sweep 정보가 없는 초기 겹침 보완
    if (!bFromSweep || HitResult.GetActor() != OtherActor || HitResult.GetComponent() != OtherComponent)
    {
        HitResult = FHitResult(OtherActor, OtherComponent, OtherComponent->GetComponentLocation(), FVector::ZeroVector);

        HitResult.Item = OtherBodyIndex;
    }

    // 관통 대상의 피해 적용 및 명중 통지
    ApplyHitDamage(HitResult);
    OnHit.Broadcast(HitResult);
}

void ATBProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 발사자 측 이동 제외 설정 복구
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
