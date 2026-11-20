#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/HitResult.h"
#include "TBProjectile.generated.h"

class UPrimitiveComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

// 충돌 결과 전달
DECLARE_MULTICAST_DELEGATE_OneParam(FTBProjectileHit, const FHitResult&);

// 투사체 종료 및 실제 발사 여부 전달
DECLARE_MULTICAST_DELEGATE_OneParam(FTBProjectileFinished, bool);

UCLASS(Blueprintable)
class DEVELROCKET_TEAMPROJ_API ATBProjectile : public AActor
{
    GENERATED_BODY()

public:
    ATBProjectile();

    FTBProjectileHit OnHit;
    FTBProjectileFinished OnFinished;

    // 구독 완료 후 서버에서 발사
    virtual bool Launch();

protected:
    // 외형과 충돌 판정을 함께 담당하는 루트 메시
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
    TObjectPtr<UStaticMeshComponent> ProjectileMesh;

    // 서버 투사체 이동
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
    TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

    // Pawn 관통 여부
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
    bool bPiercing = false;

    // 서버 투사체 수명
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0.1"))
    float LifeSeconds = 5.0f;

    virtual void BeginPlay() override;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // 차단 충돌 처리
    UFUNCTION()
    virtual void HandleCollision(
        UPrimitiveComponent* HitComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        FVector NormalImpulse,
        const FHitResult& HitResult);

    // 관통 대상과의 겹침 처리
    UFUNCTION()
    virtual void HandleOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

private:
    // 종료 시 발사자 측 이동 제외 설정 복구
    TWeakObjectPtr<UPrimitiveComponent> IgnoredSourceRoot;

    bool bLaunched = false;
    bool bResolved = false;
};