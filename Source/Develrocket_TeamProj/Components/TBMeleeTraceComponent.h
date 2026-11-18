#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/HitResult.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TBMeleeTraceComponent.generated.h"

class UPrimitiveComponent;

// 하나의 소켓 구간에 대한 판정 설정
USTRUCT(BlueprintType)
struct FTBMeleeTraceSegment
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee")
    FName StartSocket = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee")
    FName EndSocket = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee", meta = (ClampMin = "0.1"))
    float Radius = 12.0f;
};

// 타격 결과를 공격 어빌리티에 전달하는 네이티브 델리게이트
DECLARE_MULTICAST_DELEGATE_OneParam(FTBMeleeTraceHit, const FHitResult&);

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class DEVELROCKET_TEAMPROJ_API UTBMeleeTraceComponent  : public UActorComponent
{
    GENERATED_BODY()

public:
    UTBMeleeTraceComponent();

    FTBMeleeTraceHit OnHit;

    // 검사 메시 지정 및 공격 준비
    bool PrepareAttack(UPrimitiveComponent* InMesh);

    // 애니메이션의 타격 구간 제어
    void StartTrace();
    void StopTrace();

    // 공격 종료 시 전체 상태 정리
    void FinishAttack();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    
protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // 캐릭터별 소켓 이름과 개수 설정
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Melee")
    TArray<FTBMeleeTraceSegment> TraceSegments;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Melee")
    TArray<TEnumAsByte<EObjectTypeQuery>> TraceObjectTypes;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Melee")
    TEnumAsByte<EDrawDebugTrace::Type> DrawDebugType = EDrawDebugTrace::None;

private:
    TWeakObjectPtr<UPrimitiveComponent> TraceMesh;

    // 현재 판정 구간에서 이미 감지한 대상
    TSet<TWeakObjectPtr<AActor>> HitActors;

    bool bAttackPrepared = false;
    bool bTracing = false;

    // 타격 콜백 도중 판정 상태 변경 확인
    uint32 TraceRevision = 0;
};