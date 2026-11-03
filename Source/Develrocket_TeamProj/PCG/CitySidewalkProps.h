#pragma once
#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "CitySidewalkProps.generated.h"

UCLASS(BlueprintType, ClassGroup=(Procedural))
class DEVELROCKET_TEAMPROJ_API UCitySidewalkPropsSettings : public UPCGSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sidewalk", meta=(ClampMin="1", PCG_Overridable)) double Spacing = 300.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sidewalk", meta=(ClampMin="0", PCG_Overridable)) double CornerMargin = 300.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sidewalk", meta=(PCG_Overridable)) double EdgeOffset = 400.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sidewalk", meta=(PCG_Overridable)) double HeightOffset = 10.2468;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sidewalk", meta=(PCG_Overridable)) bool bAllSides = false;
#if WITH_EDITOR
    virtual FName GetDefaultNodeName() const override { return TEXT("CitySidewalkProps"); }
    virtual FText GetDefaultNodeTitle() const override { return FText::FromString(TEXT("City Sidewalk Props")); }
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif
protected:
    virtual TArray<FPCGPinProperties> InputPinProperties() const override { return DefaultPointInputPinProperties(); }
    virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
    virtual FPCGElementPtr CreateElement() const override;
};
