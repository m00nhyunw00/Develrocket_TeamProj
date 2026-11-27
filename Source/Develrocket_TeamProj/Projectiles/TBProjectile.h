#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/HitResult.h"
#include "TBProjectile.generated.h"

class ATBCharacterBase;
class UAbilitySystemComponent;
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

    // 발사 전 공격 정보 초기화
    bool InitializeAttackDamage(ATBCharacterBase* InAttacker, float InDamage, const TArray<TSubclassOf<AActor>>& InAllowedTargetClasses, bool bInRequireUnblockedAttacker);

    // 서버에서 투사체 발사
    virtual bool Launch();

    // 발사 시작 여부 조회
    bool HasLaunched() const
    {
        return bLaunched;
    }

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
    virtual void HandleCollision(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& HitResult);

    // 관통 대상과의 겹침 처리
    UFUNCTION()
    virtual void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
    // 저장된 공격 정보로 명중 피해 적용
    void ApplyHitDamage(const FHitResult& HitResult);

    // 발사 당시 공격자와 ASC
    TWeakObjectPtr<ATBCharacterBase> DamageSource;
    TWeakObjectPtr<UAbilitySystemComponent> DamageSourceASC;

    // 발사 시 확정한 피해량
    float DamageAtLaunch = 0.0f;

    // 명중 시 공격자의 행동 가능 상태 검사 여부
    bool bRequireUnblockedDamageSource = false;

    // 피해를 허용하는 대상 클래스
    UPROPERTY(Transient)
    TArray<TSubclassOf<AActor>> DamageTargetClasses;

    // 이번 투사체로 피해를 적용한 대상
    TSet<TWeakObjectPtr<AActor>> DamagedActors;

    // 종료 시 발사자 측 이동 제외 설정 복구
    TWeakObjectPtr<UPrimitiveComponent> IgnoredSourceRoot;

    // 발사 시작 여부
    bool bLaunched = false;

    // 차단 충돌 처리 완료 여부
    bool bResolved = false;
};
