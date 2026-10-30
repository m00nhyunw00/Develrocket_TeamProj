#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "CityBlockPacking.generated.h"

// 블록 포인트와 카탈로그를 입력받는 건물 크기 기반 배치 노드
UCLASS(BlueprintType, ClassGroup=(Procedural))
class DEVELROCKET_TEAMPROJ_API UCityBlockPackingSettings : public UPCGSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Packing", meta=(ClampMin="0", PCG_Overridable))
    double BuildingGap = 250;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Packing", meta=(ClampMin="0", PCG_Overridable))
    double EdgeInset = 0;

    // 포인트 검증 후 늘리는 블록별 생성 상한
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Packing", meta=(ClampMin="1", ClampMax="64", PCG_Overridable))
    int32 MaxBuildingsPerBlock = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Packing", meta=(ClampMin="1", ClampMax="64", PCG_Overridable))
    int32 LayoutAttempts = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Packing", meta=(PCG_Overridable))
    bool bAllowQuarterTurns = true;

    virtual bool UseSeed() const override { return true; }
#if WITH_EDITOR
    virtual FName GetDefaultNodeName() const override { return TEXT("CityBlockPacking"); }
    virtual FText GetDefaultNodeTitle() const override { return FText::FromString(TEXT("City Block Packing")); }
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif
protected:
    virtual TArray<FPCGPinProperties> InputPinProperties() const override;
    virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
    virtual FPCGElementPtr CreateElement() const override;
};
