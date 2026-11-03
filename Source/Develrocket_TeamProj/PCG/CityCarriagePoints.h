#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "CityCarriagePoints.generated.h"

UCLASS(BlueprintType, ClassGroup=(Procedural))
class DEVELROCKET_TEAMPROJ_API UCityCarriagePointsSettings : public UPCGSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Carriages", meta=(ClampMin="0", PCG_Overridable))
    double RoadOffset = 750.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Carriages", meta=(ClampMin="0", PCG_Overridable))
    double CornerClearance = 650.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Carriages", meta=(ClampMin="0.0", ClampMax="1.0", PCG_Overridable))
    double SpawnChance = 0.2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Carriages", meta=(PCG_Overridable))
    double HeightOffset = 0.0;

#if WITH_EDITOR
    virtual FName GetDefaultNodeName() const override { return TEXT("CityCarriagePoints"); }
    virtual FText GetDefaultNodeTitle() const override { return FText::FromString(TEXT("City Carriage Points")); }
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif
protected:
    virtual TArray<FPCGPinProperties> InputPinProperties() const override { return DefaultPointInputPinProperties(); }
    virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return DefaultPointOutputPinProperties(); }
    virtual FPCGElementPtr CreateElement() const override;
};
