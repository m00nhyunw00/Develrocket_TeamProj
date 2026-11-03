#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "CityPatrolPoints.generated.h"

UCLASS(BlueprintType, ClassGroup=(Procedural))
// 보도 네 모서리 바깥에 정찰 후보 포인트를 생성하는 PCG 노드
class DEVELROCKET_TEAMPROJ_API UCityPatrolPointsSettings : public UPCGSettings
{
    GENERATED_BODY()
public:
    // Distance beyond each sidewalk corner along both local axes.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Patrol", meta=(ClampMin="0", PCG_Overridable))
    double RoadOffset = 600.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Patrol", meta=(PCG_Overridable))
    double HeightOffset = 0.0;

#if WITH_EDITOR
    virtual FName GetDefaultNodeName() const override { return TEXT("CityPatrolPoints"); }
    virtual FText GetDefaultNodeTitle() const override { return FText::FromString(TEXT("City Patrol Points")); }
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif
protected:
    virtual TArray<FPCGPinProperties> InputPinProperties() const override { return DefaultPointInputPinProperties(); }
    virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return DefaultPointOutputPinProperties(); }
    virtual FPCGElementPtr CreateElement() const override;
};
