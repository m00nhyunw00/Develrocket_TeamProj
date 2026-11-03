#include "PCG/CitySidewalkProps.h"
#include "PCGContext.h"
#include "PCGElement.h"
#include "Data/PCGBasePointData.h"
#include "Data/PCGPointData.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"

namespace
{
    bool ReadSpan(const UPCGData* Data, const FName Name, int32 Index, double& Value)
    {
        FPCGAttributePropertyInputSelector S; S.SetAttributeName(Name);
        const auto A = PCGAttributeAccessorHelpers::CreateConstAccessor(Data, S);
        const auto K = PCGAttributeAccessorHelpers::CreateConstKeys(Data, S);
        return A && K && A->Get<double>(Value, Index, *K, EPCGAttributeAccessorFlags::AllowBroadcastAndConstructible);
    }
}

class FCitySidewalkPropsElement : public IPCGElement
{
protected:
    virtual bool SupportsBasePointDataInputs(FPCGContext*) const override { return true; }
    virtual bool ExecuteInternal(FPCGContext* Context) const override
    {
        const auto* Settings = Context->GetInputSettings<UCitySidewalkPropsSettings>();
        UPCGPointData* Lamps = NewObject<UPCGPointData>();
        UPCGPointData* Bollards = NewObject<UPCGPointData>();
        for (const FPCGTaggedData& Tagged : Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel))
        {
            const auto* Data = Cast<UPCGBasePointData>(Tagged.Data);
            if (!Data) continue;
            for (int32 Index = 0; Index < Data->GetNumPoints(); ++Index)
            {
                double X=0, Y=0;
                if (!ReadSpan(Data,TEXT("CornerSpanX"),Index,X) || !ReadSpan(Data,TEXT("CornerSpanY"),Index,Y) || X<=0 || Y<=0) continue;
                const FTransform& Frame=Data->GetTransform(Index);
                const double Spacing=FMath::Max(1.0,Settings->Spacing), Margin=FMath::Max(0.0,Settings->CornerMargin);
                for (int32 Side=0;Side<(Settings->bAllSides ? 4 : 1);++Side)
                {
                    const bool AlongX=Side>=2;
                    const double Length=AlongX ? X : Y;
                    const double Available=Length-2*Margin;
                    if (Available<0) continue;
                    // 코너 여유를 제외한 변의 중앙을 기준으로 일정 간격 배치
                    const int32 Count=FMath::Min(256,1+FMath::FloorToInt(Available/Spacing));
                    const double Start=-0.5*(Count-1)*Spacing;
                    for(int32 N=0;N<Count;++N)
                    {
                        const double Along=Start+N*Spacing;
                        const FVector Local=AlongX ? FVector(Along,(Side==2?1:-1)*(Y*0.5+Settings->EdgeOffset),Settings->HeightOffset)
                            : FVector((Side==0?1:-1)*(X*0.5+Settings->EdgeOffset),Along,Settings->HeightOffset);
                        // 각 변에서 가로등과 볼라드를 번갈아 별도 출력
                        FPCGPoint& P=((N%2)==0 ? Lamps : Bollards)->GetMutablePoints().AddDefaulted_GetRef();
                        P.Transform=FTransform(Frame.GetRotation(),Frame.TransformPositionNoScale(Local),FVector::OneVector);
                        P.Density=1;
                    }
                }
            }
        }
        auto& A=Context->OutputData.TaggedData.Emplace_GetRef(); A.Data=Lamps; A.Pin=TEXT("Lights");
        auto& B=Context->OutputData.TaggedData.Emplace_GetRef(); B.Data=Bollards; B.Pin=TEXT("Bollards");
        return true;
    }
};

TArray<FPCGPinProperties> UCitySidewalkPropsSettings::OutputPinProperties() const
{
    TArray<FPCGPinProperties> Pins; Pins.Emplace(TEXT("Lights"),EPCGDataType::Point); Pins.Emplace(TEXT("Bollards"),EPCGDataType::Point); return Pins;
}
FPCGElementPtr UCitySidewalkPropsSettings::CreateElement() const { return MakeShared<FCitySidewalkPropsElement>(); }
