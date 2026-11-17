#pragma once

#include "CoreMinimal.h"
#include "CollisionQueryParams.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "TBMeleeTraceComponent.generated.h"

class UPrimitiveComponent;
class USkeletalMeshComponent;

// 하나의 소켓 구간에 대한 판정 설정
USTRUCT(BlueprintType)
struct FTBMeleeTraceSegment
{
    GENERATED_BODY()

    // 실제 메시를 찾을 때 사용할 등록 이름
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee")
    FName MeshKey = TEXT("Body");

    // 검사 구간의 시작 소켓
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee")
    FName StartSocket = NAME_None;

    // 검사 구간의 끝 소켓
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee")
    FName EndSocket = NAME_None;

    // 소켓 사이를 이동시키는 구의 반지름
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee", meta = (ClampMin = "0.1"))
    float Radius = 12.0f;
};

// 한 공격에서 함께 사용할 여러 판정 구간
USTRUCT(BlueprintType)
struct FTBMeleeTraceProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee")
    TArray<FTBMeleeTraceSegment> Segments;
};

// 새로운 대상과 접촉했을 때 전달할 타격 정보
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTBMeleeTraceHit, const FHitResult&, HitResult);

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class DEVELROCKET_TEAMPROJ_API UTBMeleeTraceComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UTBMeleeTraceComponent();

    // 실제 피해 처리는 이 알림을 구독한 어빌리티에서 수행
    UPROPERTY(BlueprintAssignable, Category = "Combat|Melee")
    FTBMeleeTraceHit OnHit;

    // Body·MainWeapon 등의 이름에 실제 메시 연결
    UFUNCTION(BlueprintCallable, Category = "Combat|Melee")
    void SetTraceMesh(FName MeshKey, UPrimitiveComponent* Mesh);

    // 설정 검증 후 판정 구간 시작
    bool BeginWindow(FName ProfileName, USkeletalMeshComponent* SourceMesh, const UObject* WindowOwner, int32 MontageInstanceID);

    // 현재 구간과 일치하는 종료 요청만 처리
    void EndWindow(USkeletalMeshComponent* SourceMesh, const UObject* WindowOwner, int32 MontageInstanceID);

    // 어빌리티 취소·장비 교체 등에서 강제 종료
    UFUNCTION(BlueprintCallable, Category = "Combat|Melee")
    void StopAllTraces();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // 캐릭터 BP마다 다른 소켓 구성 지정
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Melee")
    TMap<FName, FTBMeleeTraceProfile> TraceProfiles;

    // 피해 후보를 찾을 ObjectType 목록
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Melee")
    TArray<TEnumAsByte<EObjectTypeQuery>> TargetObjectTypes;

    // 빠른 이동 시 프레임 사이를 나누어 검사할 최대 횟수
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Melee", meta = (ClampMin = "1", ClampMax = "8"))
    int32 MaxSubsteps = 4;

    // 공격자의 시점에서 피격 지점까지 장애물 확인
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Melee")
    bool bCheckOcclusion = true;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Melee")
    TEnumAsByte<ECollisionChannel> OcclusionChannel = ECC_Visibility;

private:
    // 이번 실행에서 사용할 메시와 이전 소켓 위치
    struct FRunningTrace
    {
        FTBMeleeTraceSegment Settings;
        TWeakObjectPtr<UPrimitiveComponent> Mesh;
        FVector PreviousStart = FVector::ZeroVector;
        FVector PreviousEnd = FVector::ZeroVector;
    };

    // 스윕 결과 전달 중 판정이 종료되면 false 반환
    bool SweepSegment(const FVector& Start, const FVector& End, float Radius, uint32 ExpectedGeneration);

    // 장착 상태에 따라 등록한 실제 메시
    TMap<FName, TWeakObjectPtr<UPrimitiveComponent>> TraceMeshes;

    // 현재 구간의 검사 목록과 중복 타격 기록
    TArray<FRunningTrace> RunningTraces;
    TSet<TWeakObjectPtr<AActor>> HitActors;

    // 이번 실행의 충돌 검사 조건
    FCollisionQueryParams ActiveQueryParams;
    FCollisionObjectQueryParams ActiveObjectParams;

    // 판정을 시작한 애니메이션 실행 정보
    TWeakObjectPtr<USkeletalMeshComponent> ActiveSourceMesh;
    TWeakObjectPtr<const UObject> ActiveWindowOwner;
    int32 ActiveMontageInstanceID = INDEX_NONE;

    // 콜백 내부에서 종료·재시작된 실행 구분
    uint32 Generation = 0;
    bool bTracing = false;
};