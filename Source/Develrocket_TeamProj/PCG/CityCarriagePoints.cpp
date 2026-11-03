#include "PCG/CityCarriagePoints.h"

#include "Data/PCGBasePointData.h"
#include "Data/PCGPointData.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "PCGContext.h"
#include "PCGElement.h"

namespace
{
    bool ReadCarriageSpan(const UPCGData* Data, FName Name, int32 Index, double& Value)
    {
        FPCGAttributePropertyInputSelector Selector;
        Selector.SetAttributeName(Name);
        const auto Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(Data, Selector);
        const auto Keys = PCGAttributeAccessorHelpers::CreateConstKeys(Data, Selector);
        return Accessor && Keys && Accessor->Get<double>(Value, Index, *Keys, EPCGAttributeAccessorFlags::AllowBroadcastAndConstructible);
    }
}

class FCityCarriagePointsElement : public IPCGElement
{
protected:
    virtual bool SupportsBasePointDataInputs(FPCGContext*) const override { return true; }
    virtual bool ExecuteInternal(FPCGContext* Context) const override
    {
        const auto* Settings = Context->GetInputSettings<UCityCarriagePointsSettings>();
        for (const FPCGTaggedData& Tagged : Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel))
        {
            const auto* Data = Cast<UPCGBasePointData>(Tagged.Data);
            if (!Data) continue;
            UPCGPointData* Output = NewObject<UPCGPointData>();
            Output->InitializeFromData(Data);
            for (int32 Index = 0; Index < Data->GetNumPoints(); ++Index)
            {
                double SpanX = 0.0, SpanY = 0.0;
                if (!ReadCarriageSpan(Data, TEXT("CornerSpanX"), Index, SpanX) ||
                    !ReadCarriageSpan(Data, TEXT("CornerSpanY"), Index, SpanY) || SpanX <= 0.0 || SpanY <= 0.0) continue;
                const FTransform& Frame = Data->GetTransform(Index);
                for (int32 Side = 0; Side < 4; ++Side)
                {
                    const bool bAlongX = Side >= 2;
                    const double Length = bAlongX ? SpanX : SpanY;
                    if (Length < 2.0 * Settings->CornerClearance) continue;
                    const FVector Center = Frame.GetLocation();
                    uint32 Seed = HashCombine(GetTypeHash(Context->GetSeed()), GetTypeHash(FMath::RoundToInt(Center.X)));
                    Seed = HashCombine(Seed, GetTypeHash(FMath::RoundToInt(Center.Y)));
                    Seed = HashCombine(Seed, GetTypeHash(Side));
                    FRandomStream Random(static_cast<int32>(Seed));
                    if (Random.FRand() >= FMath::Clamp(Settings->SpawnChance, 0.0, 1.0)) continue;
                    const double Along = Random.FRandRange(-0.5 * Length + Settings->CornerClearance, 0.5 * Length - Settings->CornerClearance);
                    const double Sign = (Side % 2 == 0) ? 1.0 : -1.0;
                    const FVector Local = bAlongX ? FVector(Along, Sign * (SpanY * 0.5 + Settings->RoadOffset), Settings->HeightOffset)
                                                  : FVector(Sign * (SpanX * 0.5 + Settings->RoadOffset), Along, Settings->HeightOffset);
                    FPCGPoint& Point = Output->GetMutablePoints().AddDefaulted_GetRef();
                    const FQuat Rotation = Frame.GetRotation() * FRotator(0.0, bAlongX ? 0.0 : 90.0, 0.0).Quaternion();
                    Point.Transform = FTransform(Rotation, Frame.TransformPositionNoScale(Local), FVector::OneVector);
                    Point.BoundsMin = FVector(-500.0, -220.0, -10.0);
                    Point.BoundsMax = FVector(500.0, 220.0, 400.0);
                    Point.Seed = static_cast<int32>(Seed);
                    Point.Density = 1.0;
                }
            }
            FPCGTaggedData& Out = Context->OutputData.TaggedData.Emplace_GetRef(Tagged);
            Out.Data = Output;
            Out.Pin = PCGPinConstants::DefaultOutputLabel;
        }
        return true;
    }
};

FPCGElementPtr UCityCarriagePointsSettings::CreateElement() const
{
    return MakeShared<FCityCarriagePointsElement>();
}
