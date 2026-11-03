#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "CityPatrolCluster.generated.h"

UCLASS(BlueprintType, ClassGroup=(Procedural))
// 인접한 정찰 후보를 연결된 그룹으로 묶고 그룹별 평균 위치 출력
class DEVELROCKET_TEAMPROJ_API UCityPatrolClusterSettings : public UPCGSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Patrol", meta=(ClampMin="0", PCG_Overridable))
    double MergeDistance = 600.0;

#if WITH_EDITOR
    virtual FName GetDefaultNodeName() const override { return TEXT("CityPatrolCluster"); }
    virtual FText GetDefaultNodeTitle() const override { return FText::FromString(TEXT("City Patrol Cluster")); }
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif
protected:
    virtual TArray<FPCGPinProperties> InputPinProperties() const override { return DefaultPointInputPinProperties(); }
    virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return DefaultPointOutputPinProperties(); }
    virtual FPCGElementPtr CreateElement() const override;
};
