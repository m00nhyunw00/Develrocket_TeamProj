#include "PCG/CityPatrolPoints.h"

#include "Data/PCGBasePointData.h"
#include "Data/PCGPointData.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "PCGContext.h"
#include "PCGElement.h"

namespace
{
    bool ReadPatrolSpan(const UPCGData* Data, const FName Name, int32 Index, double& Value)
    {
        FPCGAttributePropertyInputSelector Selector;
        Selector.SetAttributeName(Name);
        const auto Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(Data, Selector);
        const auto Keys = PCGAttributeAccessorHelpers::CreateConstKeys(Data, Selector);
        return Accessor && Keys && Accessor->Get<double>(Value, Index, *Keys, EPCGAttributeAccessorFlags::AllowBroadcastAndConstructible);
    }
}

class FCityPatrolPointsElement : public IPCGElement
{
protected:
    virtual bool SupportsBasePointDataInputs(FPCGContext*) const override { return true; }
    virtual bool ExecuteInternal(FPCGContext* Context) const override
    {
        const auto* Settings = Context->GetInputSettings<UCityPatrolPointsSettings>();
        for (const FPCGTaggedData& Tagged : Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel))
        {
            const auto* Data = Cast<UPCGBasePointData>(Tagged.Data);
            if (!Data) continue;
            UPCGPointData* Output = NewObject<UPCGPointData>();
            Output->InitializeFromData(Data);
            for (int32 Index = 0; Index < Data->GetNumPoints(); ++Index)
            {
                double SpanX = 0.0, SpanY = 0.0;
                if (!ReadPatrolSpan(Data, TEXT("CornerSpanX"), Index, SpanX) ||
                    !ReadPatrolSpan(Data, TEXT("CornerSpanY"), Index, SpanY) || SpanX <= 0.0 || SpanY <= 0.0) continue;
                const FTransform& Frame = Data->GetTransform(Index);
                for (int32 XSign : {-1, 1})
                {
                    for (int32 YSign : {-1, 1})
                    {
                        const FVector Local(XSign * (SpanX * 0.5 + Settings->RoadOffset),
                                            YSign * (SpanY * 0.5 + Settings->RoadOffset), Settings->HeightOffset);
                        FPCGPoint& Point = Output->GetMutablePoints().AddDefaulted_GetRef();
                        Point.Transform = FTransform(Frame.GetRotation(), Frame.TransformPositionNoScale(Local), FVector::OneVector);
                        Point.Density = 1.0;
                    }
                }
            }
            FPCGTaggedData& Out = Context->OutputData.TaggedData.Emplace_GetRef(Tagged);
            Out.Data = Output;
            Out.Pin = PCGPinConstants::DefaultOutputLabel;
        }
        return true;
    }
};

FPCGElementPtr UCityPatrolPointsSettings::CreateElement() const
{
    return MakeShared<FCityPatrolPointsElement>();
}
