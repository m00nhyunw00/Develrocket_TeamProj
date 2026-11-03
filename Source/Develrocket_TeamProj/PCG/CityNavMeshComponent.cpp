#include "PCG/CityNavMeshComponent.h"

#include "AI/Navigation/NavigationDirtyArea.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCityNavigation, Log, All);

UCityNavMeshComponent::UCityNavMeshComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UCityNavMeshComponent::InvalidateCityNavigation()
{
    if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(PollTimer);
    bCityNavigationReady = false;
    ReadyRevision = 0;
    PendingRevision = 0;
    PendingCityVolume.Reset();
    bDirtyAreaRequested = false;
    IdlePollCount = 0;
}

void UCityNavMeshComponent::RequestCityNavigationUpdate(AActor* CityVolume, int32 Revision)
{
    InvalidateCityNavigation();
    if (!GetOwner() || !GetOwner()->HasAuthority() || !GetWorld()) return;
    PendingRevision = Revision;
    PendingCityVolume = CityVolume;
    StartTime = GetWorld()->GetTimeSeconds();
    if (!IsValid(CityVolume) || Revision <= 0)
    {
        FailNavigationUpdate(TEXT("Invalid city volume or layout revision."));
        return;
    }
    // PCG가 만든 컴포넌트의 내비게이션 등록 처리 대기
    GetWorld()->GetTimerManager().SetTimer(PollTimer, this, &UCityNavMeshComponent::PollNavigationUpdate, 0.5f, true, 0.5f);
}

void UCityNavMeshComponent::PollNavigationUpdate()
{
    UWorld* World = GetWorld();
    AActor* CityVolume = PendingCityVolume.Get();
    if (!World || !CityVolume || !GetOwner() || !GetOwner()->HasAuthority())
    {
        FailNavigationUpdate(TEXT("City or world is no longer available."));
        return;
    }
    if (World->GetTimeSeconds() - StartTime > FMath::Max(1.0f, BuildTimeoutSeconds))
    {
        FailNavigationUpdate(TEXT("Navigation update timed out. Check navigation bounds and build locks."));
        return;
    }

    UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    ANavigationData* NavData = NavSystem ? NavSystem->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) : nullptr;
    if (!NavSystem || !NavData) return;

    if (!bDirtyAreaRequested)
    {
        if (NavData->GetRuntimeGenerationMode() != ERuntimeGenerationType::Dynamic)
        {
            FailNavigationUpdate(TEXT("NavMesh Runtime Generation must be Dynamic for a runtime-generated city."));
            return;
        }
        const FBox CityBounds = CityVolume->GetComponentsBoundingBox(false, false);
        // 도시 영역과 NavMesh 영역의 교차 여부 확인 후 갱신 요청
        TArray<FBox> NavBounds;
        NavSystem->GetNavigationBoundsForNavData(*NavData, NavBounds);
        bool bOverlaps = false;
        for (const FBox& Bounds : NavBounds) bOverlaps |= Bounds.Intersect(CityBounds);
        if (!CityBounds.IsValid || !bOverlaps)
        {
            FailNavigationUpdate(TEXT("No NavMeshBoundsVolume overlaps the PCG city volume."));
            return;
        }
        NavSystem->AddDirtyArea(CityBounds, ENavigationDirtyFlag::All, TEXT("City PCG generated"));
        bDirtyAreaRequested = true;
        UE_LOG(LogCityNavigation, Display, TEXT("Requested navigation update for city revision %d."), PendingRevision);
        return;
    }

    // 생성 작업과 대기 중인 dirty 영역이 끝난 뒤 안정 상태 확인
    if (UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World))
    {
        IdlePollCount = 0;
        return;
    }
    if (++IdlePollCount < 2) return;
    World->GetTimerManager().ClearTimer(PollTimer);
    bCityNavigationReady = true;
    ReadyRevision = PendingRevision;
    UE_LOG(LogCityNavigation, Display, TEXT("Navigation update settled for city revision %d."), ReadyRevision);
    OnCityNavigationReady.Broadcast(ReadyRevision);
}

void UCityNavMeshComponent::FailNavigationUpdate(const FString& Reason)
{
    const int32 FailedRevision = PendingRevision;
    InvalidateCityNavigation();
    UE_LOG(LogCityNavigation, Warning, TEXT("Navigation update failed for city revision %d: %s"), FailedRevision, *Reason);
    OnCityNavigationFailed.Broadcast(FailedRevision, Reason);
}

void UCityNavMeshComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    InvalidateCityNavigation();
    Super::EndPlay(EndPlayReason);
}
