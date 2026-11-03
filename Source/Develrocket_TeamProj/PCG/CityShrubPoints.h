#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "CityShrubPoints.generated.h"

// 보도 내부의 노출된 흙바닥에서 관목 배치 후보 생성
UCLASS(BlueprintType, ClassGroup=(Procedural))
class DEVELROCKET_TEAMPROJ_API UCityShrubPointsSettings : public UPCGSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shrubs", meta=(ClampMin="0", ClampMax="32", PCG_Overridable))
    int32 MaxShrubsPerLot = 24;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shrubs", meta=(ClampMin="0", PCG_Overridable))
    double EdgeMargin = 350.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shrubs", meta=(ClampMin="0", PCG_Overridable))
    double MinSpacing = 220.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shrubs", meta=(ClampMin="0", PCG_Overridable))
    double Clearance = 100.0;

    virtual bool UseSeed() const override { return true; }
#if WITH_EDITOR
    virtual FName GetDefaultNodeName() const override { return TEXT("CityShrubPoints"); }
    virtual FText GetDefaultNodeTitle() const override { return FText::FromString(TEXT("City Shrub Points")); }
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif
protected:
    virtual TArray<FPCGPinProperties> InputPinProperties() const override { return DefaultPointInputPinProperties(); }
    virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
    virtual FPCGElementPtr CreateElement() const override;
};
