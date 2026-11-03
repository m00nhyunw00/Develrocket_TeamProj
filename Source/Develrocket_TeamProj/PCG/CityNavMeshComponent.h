#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CityNavMeshComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCityNavigationReady, int32, Revision);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCityNavigationFailed, int32, Revision, const FString&, Reason);

UCLASS(ClassGroup=(City), meta=(BlueprintSpawnableComponent))
// 서버의 도시 생성 이후 동적 NavMesh 갱신과 안정 상태 알림 관리
class DEVELROCKET_TEAMPROJ_API UCityNavMeshComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UCityNavMeshComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Navigation", meta=(ClampMin="1.0"))
    float BuildTimeoutSeconds = 60.0f;

    UPROPERTY(BlueprintReadOnly, VisibleInstanceOnly, Category="City|Navigation")
    bool bCityNavigationReady = false;

    UPROPERTY(BlueprintReadOnly, VisibleInstanceOnly, Category="City|Navigation")
    int32 ReadyRevision = 0;

    // 갱신 작업의 안정 상태 알림; 개별 위치의 경로 접근성 검증은 별도 처리
    UPROPERTY(BlueprintAssignable, Category="City|Navigation")
    FCityNavigationReady OnCityNavigationReady;

    UPROPERTY(BlueprintAssignable, Category="City|Navigation")
    FCityNavigationFailed OnCityNavigationFailed;

    void InvalidateCityNavigation();
    void RequestCityNavigationUpdate(AActor* CityVolume, int32 Revision);

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void PollNavigationUpdate();
    void FailNavigationUpdate(const FString& Reason);

    FTimerHandle PollTimer;
    TWeakObjectPtr<AActor> PendingCityVolume;
    int32 PendingRevision = 0;
    double StartTime = 0.0;
    bool bDirtyAreaRequested = false;
    int32 IdlePollCount = 0;
};
