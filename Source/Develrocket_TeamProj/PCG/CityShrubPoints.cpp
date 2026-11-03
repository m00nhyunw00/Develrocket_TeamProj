#include "PCG/CityShrubPoints.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGElement.h"
#include "Data/PCGBasePointData.h"
#include "Data/PCGPointData.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

namespace
{
    bool ReadShrubSpan(const UPCGData* Data, FName Name, int32 Index, double& Value)
    {
        FPCGAttributePropertyInputSelector Selector;
        Selector.SetAttributeName(Name);
        const auto Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(Data, Selector);
        const auto Keys = PCGAttributeAccessorHelpers::CreateConstKeys(Data, Selector);
        return Accessor && Keys && Accessor->Get<double>(Value, Index, *Keys, EPCGAttributeAccessorFlags::AllowBroadcastAndConstructible);
    }
}

class FCityShrubPointsElement : public IPCGElement
{
public:
    virtual bool CanExecuteOnlyOnMainThread(FPCGContext*) const override { return true; }
    virtual bool IsCacheable(const UPCGSettings*) const override { return false; }
protected:
    virtual bool SupportsBasePointDataInputs(FPCGContext*) const override { return true; }
    virtual bool ExecuteInternal(FPCGContext* Context) const override
    {
        const auto* Settings = Context->GetInputSettings<UCityShrubPointsSettings>();
        const UPCGComponent* Component = Cast<UPCGComponent>(Context->ExecutionSource.GetObject());
        UWorld* World = Component ? Component->GetWorld() : nullptr;
        if (!World) return true;
        for (const FPCGTaggedData& Tagged : Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel))
        {
            const auto* Data = Cast<UPCGBasePointData>(Tagged.Data);
            if (!Data) continue;
            UPCGPointData* Outputs[4];
            for (UPCGPointData*& Output : Outputs)
            {
                Output = NewObject<UPCGPointData>();
                Output->InitializeFromData(Data);
            }
            for (int32 Index = 0; Index < Data->GetNumPoints(); ++Index)
            {
                double SpanX = 0.0, SpanY = 0.0;
                if (!ReadShrubSpan(Data, TEXT("CornerSpanX"), Index, SpanX) ||
                    !ReadShrubSpan(Data, TEXT("CornerSpanY"), Index, SpanY)) continue;
                const double HalfX = SpanX * 0.5 - Settings->EdgeMargin;
                const double HalfY = SpanY * 0.5 - Settings->EdgeMargin;
                if (HalfX <= 0 || HalfY <= 0) continue;
                const FTransform& Frame = Data->GetTransform(Index);
                const FVector Center = Frame.GetLocation();
                uint32 Seed = HashCombine(GetTypeHash(Context->GetSeed()), GetTypeHash(FMath::RoundToInt(Center.X)));
                Seed = HashCombine(Seed, GetTypeHash(FMath::RoundToInt(Center.Y)));
                FRandomStream Random(static_cast<int32>(Seed));
                TArray<FVector> Accepted;
                for (int32 Attempt = 0; Attempt < 600 && Accepted.Num() < FMath::Clamp(Settings->MaxShrubsPerLot, 0, 32); ++Attempt)
                {
                    const FVector Local(Random.FRandRange(-HalfX, HalfX), Random.FRandRange(-HalfY, HalfY), 0);
                    const FVector Position = Frame.TransformPositionNoScale(Local);
                    bool bNear = false;
                    for (const FVector& Existing : Accepted)
                        if (FVector::DistSquared2D(Position, Existing) < FMath::Square(Settings->MinSpacing)) { bNear = true; break; }
                    if (bNear) continue;
                    // 큰 관목을 먼저 선택하고 작은 관목을 섞어 서로 다른 높이와 폭 구성
                    constexpr int32 Variants[] = {0, 1, 2, 3};
                    const int32 Variant = Variants[Accepted.Num() % UE_ARRAY_COUNT(Variants)];
                    const double Clearance = FMath::Max(Settings->Clearance, Variant < 2 ? 200.0 : 100.0);
                    bool bOnDirt = true;
                    FVector Ground = FVector::ZeroVector;
                    for (const FVector2D Offset : {FVector2D(0, 0), FVector2D(Clearance, 0), FVector2D(-Clearance, 0), FVector2D(0, Clearance), FVector2D(0, -Clearance)})
                    {
                        FHitResult Hit;
                        const FVector Test = Position + Frame.GetRotation().RotateVector(FVector(Offset.X, Offset.Y, 0));
                        FCollisionQueryParams Params(SCENE_QUERY_STAT(CityShrubGround), false);
                        if (!World->LineTraceSingleByChannel(Hit, Test + FVector(0, 0, 10000), Test - FVector(0, 0, 500), ECC_Visibility, Params)
                            || !Hit.GetComponent() || Hit.GetComponent()->GetName() != TEXT("CenterPlane")
                            || !Hit.GetActor() || !Hit.GetActor()->GetClass()->GetName().Contains(TEXT("CitySidewalk_Assembly")))
                        {
                            bOnDirt = false;
                            break;
                        }
                        if (Offset.IsNearlyZero()) Ground = Hit.ImpactPoint;
                    }
                    if (!bOnDirt) continue;
                    Accepted.Add(Ground);
                    FPCGPoint& Point = Outputs[Variant]->GetMutablePoints().AddDefaulted_GetRef();
                    Point.Transform = FTransform(FRotator(0, Random.FRandRange(0, 360), 0), Ground, FVector::OneVector);
                    Point.Seed = Random.RandHelper(MAX_int32);
                    Point.Density = 1.0;
                }
            }
            const FName Pins[] = {TEXT("A"), TEXT("B"), TEXT("C"), TEXT("D")};
            for (int32 Variant = 0; Variant < 4; ++Variant)
            {
                FPCGTaggedData& Out = Context->OutputData.TaggedData.Emplace_GetRef(Tagged);
                Out.Data = Outputs[Variant];
                Out.Pin = Pins[Variant];
            }
        }
        return true;
    }
};

TArray<FPCGPinProperties> UCityShrubPointsSettings::OutputPinProperties() const
{
    TArray<FPCGPinProperties> Pins;
    for (const FName Name : {FName(TEXT("A")), FName(TEXT("B")), FName(TEXT("C")), FName(TEXT("D"))})
        Pins.Emplace(Name, EPCGDataType::Point);
    return Pins;
}

FPCGElementPtr UCityShrubPointsSettings::CreateElement() const
{
    return MakeShared<FCityShrubPointsElement>();
}
