#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "CityStreetlightChannels.generated.h"

// PCG가 생성한 가로등 ISM에 동적 조명 채널 적용
UCLASS(BlueprintType, ClassGroup=(Procedural))
class DEVELROCKET_TEAMPROJ_API UCityStreetlightChannelsSettings : public UPCGSettings
{
    GENERATED_BODY()
public:
#if WITH_EDITOR
    virtual FName GetDefaultNodeName() const override { return TEXT("CityStreetlightChannels"); }
    virtual FText GetDefaultNodeTitle() const override { return FText::FromString(TEXT("City Streetlight Channels")); }
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif
protected:
    virtual TArray<FPCGPinProperties> InputPinProperties() const override { return DefaultPointInputPinProperties(); }
    virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return DefaultPointOutputPinProperties(); }
    virtual FPCGElementPtr CreateElement() const override;
};
