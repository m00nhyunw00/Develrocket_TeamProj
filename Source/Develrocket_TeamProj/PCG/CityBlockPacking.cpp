#include "PCG/CityBlockPacking.h"
#include "PCG/CityPackingAlgorithm.h"

#include "PCGContext.h"
#include "PCGElement.h"
#include "PCGParamData.h"
#include "Data/PCGBasePointData.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "GameFramework/Actor.h"
#include "UObject/SoftObjectPath.h"

namespace CityBlockPacking
{
    const FName BlocksPin = TEXT("Blocks");
    const FName BuildingsPin = TEXT("Buildings");
    const FName FootprintsPin = TEXT("Footprints");

    template<typename T>
    bool Read(const UPCGData* Data, FName Name, int32 Index, T& Value)
    {
        FPCGAttributePropertyInputSelector Selector;
        Selector.SetAttributeName(Name);
        const auto Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(Data, Selector);
        const auto Keys = PCGAttributeAccessorHelpers::CreateConstKeys(Data, Selector);
        return Accessor && Keys && Accessor->Get<T>(Value, Index, *Keys, EPCGAttributeAccessorFlags::AllowBroadcastAndConstructible);
    }

    struct FCatalogEntry
    {
        FCityBuildingFootprint Footprint;
        FSoftClassPath ClassPath;
        double GroundOffsetZ = 0;
        double Weight = 1;
    };

    // 블록의 표면 좌표계에서 회전된 건물 중심과 접지면을 보정한 피벗 계산
    FTransform PlacementTransform(const FTransform& Block, const FVector& LocalMin, double TopZ,
        const FCatalogEntry& Entry, const CityPacking::FPlacement& Placement, double Gap, double Inset)
    {
        const FQuat Yaw(FVector::UpVector, FMath::DegreesToRadians(Placement.Quarter * 90.0));
        const FVector Center = (Entry.Footprint.BoundsMin + Entry.Footprint.BoundsMax) * 0.5;
        const FVector RotatedCenter = Yaw.RotateVector(FVector(Center.X, Center.Y, 0));
        FVector LocalPivot(LocalMin.X + Inset + Placement.Rect.X + (Placement.Rect.W - Gap) * 0.5,
            LocalMin.Y + Inset + Placement.Rect.Y + (Placement.Rect.H - Gap) * 0.5,
            TopZ + Entry.GroundOffsetZ);
        LocalPivot -= RotatedCenter;
        return FTransform(Block.GetRotation() * Yaw, Block.TransformPosition(LocalPivot), FVector::OneVector);
    }

    struct FOutputAttributes
    {
        FPCGMetadataAttribute<FSoftClassPath>* Class;
        FPCGMetadataAttribute<FVector>* BoundsMin;
        FPCGMetadataAttribute<FVector>* BoundsMax;
        FPCGMetadataAttribute<double>* Ground;
        FPCGMetadataAttribute<int32>* BlockID;
        FPCGMetadataAttribute<int32>* LotID;
        FPCGMetadataAttribute<int32>* BlockSeed;
        FPCGMetadataAttribute<int64>* SelectionSeed;
        FPCGMetadataAttribute<int32>* QuarterTurns;
        FPCGMetadataAttribute<double>* Coverage;

        explicit FOutputAttributes(UPCGMetadata* M)
        {
            Class = M->CreateAttribute<FSoftClassPath>(TEXT("BuildingClass"), FSoftClassPath(), false, true);
            BoundsMin = M->CreateAttribute<FVector>(TEXT("BoundsMin"), FVector::ZeroVector, false, true);
            BoundsMax = M->CreateAttribute<FVector>(TEXT("BoundsMax"), FVector::ZeroVector, false, true);
            Ground = M->CreateAttribute<double>(TEXT("GroundOffsetZ"), 0, false, true);
            BlockID = M->CreateAttribute<int32>(TEXT("BlockID"), 0, false, true);
            LotID = M->CreateAttribute<int32>(TEXT("LotID"), 0, false, true);
            BlockSeed = M->CreateAttribute<int32>(TEXT("BlockSeed"), 0, false, true);
            SelectionSeed = M->CreateAttribute<int64>(TEXT("SelectionSeed"), 0, false, true);
            QuarterTurns = M->CreateAttribute<int32>(TEXT("QuarterTurns"), 0, false, true);
            Coverage = M->CreateAttribute<double>(TEXT("BlockCoverage"), 0, false, true);
        }

        void Set(PCGMetadataEntryKey Key, const FCatalogEntry& Entry, int32 InBlockID,
            int32 InLotID, int32 Seed, int32 Quarter, double InCoverage) const
        {
            Class->SetValue(Key, Entry.ClassPath);
            BoundsMin->SetValue(Key, Entry.Footprint.BoundsMin);
            BoundsMax->SetValue(Key, Entry.Footprint.BoundsMax);
            Ground->SetValue(Key, Entry.GroundOffsetZ);
            BlockID->SetValue(Key, InBlockID);
            LotID->SetValue(Key, InLotID);
            BlockSeed->SetValue(Key, Seed);
            SelectionSeed->SetValue(Key, static_cast<int64>(static_cast<uint32>(PCGHelpers::ComputeSeed(Seed, InLotID))));
            QuarterTurns->SetValue(Key, Quarter);
            Coverage->SetValue(Key, InCoverage);
        }
    };
}

class FCityBlockPackingElement : public IPCGElement
{
public:
    virtual bool CanExecuteOnlyOnMainThread(FPCGContext*) const override { return true; }
protected:
    virtual bool SupportsBasePointDataInputs(FPCGContext*) const override { return true; }
    virtual bool ExecuteInternal(FPCGContext* Context) const override
    {
        using namespace CityBlockPacking;
        const UCityBlockPackingSettings* Settings = Context->GetInputSettings<UCityBlockPackingSettings>();
        TArray<FCatalogEntry> Catalog;
        int32 RejectedRows = 0;
        for (const FPCGTaggedData& Tagged : Context->InputData.GetInputsByPin(BuildingsPin))
        {
            const UPCGParamData* Data = Cast<UPCGParamData>(Tagged.Data);
            if (!Data) continue;
            const FPCGAttributePropertyInputSelector Selector = FPCGAttributePropertyInputSelector::CreateAttributeSelector<FPCGAttributePropertyInputSelector>(TEXT("BuildingClass"));
            const auto Keys = PCGAttributeAccessorHelpers::CreateConstKeys(Data, Selector);
            if (!Keys) continue;
            for (int32 Index = 0; Index < Keys->GetNum(); ++Index)
            {
                FCatalogEntry Entry;
                FString Path;
                const bool bValid = Read(Data, TEXT("BuildingClass"), Index, Path)
                    && Read(Data, TEXT("BoundsMin"), Index, Entry.Footprint.BoundsMin)
                    && Read(Data, TEXT("BoundsMax"), Index, Entry.Footprint.BoundsMax)
                    && Read(Data, TEXT("GroundOffsetZ"), Index, Entry.GroundOffsetZ);
                Read(Data, TEXT("Weight"), Index, Entry.Weight);
                const FVector Size = Entry.Footprint.BoundsMax - Entry.Footprint.BoundsMin;
                if (!bValid || Entry.Footprint.BoundsMin.ContainsNaN() || Entry.Footprint.BoundsMax.ContainsNaN()
                    || Size.X <= 0 || Size.Y <= 0 || Size.Z < 0 || !FMath::IsFinite(Entry.GroundOffsetZ)
                    || !FMath::IsFinite(Entry.Weight) || Entry.Weight <= 0)
                {
                    ++RejectedRows;
                    continue;
                }
                Entry.ClassPath = FSoftClassPath(Path);
                Entry.Footprint.BuildingClass = Entry.ClassPath.TryLoadClass<AActor>();
                if (!Entry.Footprint.BuildingClass) { ++RejectedRows; continue; }
                Catalog.Add(MoveTemp(Entry));
            }
        }
        // 로드 순서와 무관한 카탈로그 순서 고정
        Catalog.StableSort([](const FCatalogEntry& A, const FCatalogEntry& B)
        {
            const FString AKey = FString::Printf(TEXT("%s|%s|%s|%.17g|%.17g"), *A.ClassPath.ToString(),
                *A.Footprint.BoundsMin.ToString(), *A.Footprint.BoundsMax.ToString(), A.GroundOffsetZ, A.Weight);
            const FString BKey = FString::Printf(TEXT("%s|%s|%s|%.17g|%.17g"), *B.ClassPath.ToString(),
                *B.Footprint.BoundsMin.ToString(), *B.Footprint.BoundsMax.ToString(), B.GroundOffsetZ, B.Weight);
            return AKey < BKey;
        });
        if (Catalog.IsEmpty())
        {
            UE_LOG(LogTemp, Warning, TEXT("CityBlockPacking: no valid building rows; rejected=%d. Check class, bounds, GroundOffsetZ and Weight."), RejectedRows);
            return true;
        }
        TArray<FCityBuildingFootprint> Types;
        TArray<double> Weights;
        for (const FCatalogEntry& Entry : Catalog) { Types.Add(Entry.Footprint); Weights.Add(Entry.Weight); }

        UPCGPointData* Placements = NewObject<UPCGPointData>();
        UPCGPointData* Footprints = NewObject<UPCGPointData>();
        FOutputAttributes PlacementAttributes(Placements->Metadata);
        FOutputAttributes FootprintAttributes(Footprints->Metadata);
        const double Gap = FMath::Max(0.0, Settings->BuildingGap);
        const double Inset = FMath::Max(0.0, Settings->EdgeInset);
        int32 FallbackBlockID = 0;
        int32 BlockCount = 0;
        for (const FPCGTaggedData& Tagged : Context->InputData.GetInputsByPin(BlocksPin))
        {
            const UPCGBasePointData* Blocks = Cast<UPCGBasePointData>(Tagged.Data);
            if (!Blocks) continue;
            for (int32 Index = 0; Index < Blocks->GetNumPoints(); ++Index)
            {
                const FTransform& Frame = Blocks->GetTransform(Index);
                const FVector& Min = Blocks->GetBoundsMin(Index);
                const FVector& Max = Blocks->GetBoundsMax(Index);
                int32 BlockID = FallbackBlockID++;
                Read(Blocks, TEXT("BlockID"), Index, BlockID);
                const FRotator Rotation = Frame.Rotator();
                if (!Frame.GetScale3D().Equals(FVector::OneVector, 0.0001)
                    || !FMath::IsNearlyZero(Rotation.Pitch, 0.01) || !FMath::IsNearlyZero(Rotation.Roll, 0.01))
                {
                    UE_LOG(LogTemp, Warning, TEXT("CityBlockPacking: BlockID=%d requires unit scale and a horizontal frame. Use Apply Scale To Bounds first."), BlockID);
                    continue;
                }
                const double W = Max.X - Min.X - 2 * Inset;
                const double H = Max.Y - Min.Y - 2 * Inset;
                if (Min.ContainsNaN() || Max.ContainsNaN() || W <= 0 || H <= 0) continue;
                ++BlockCount;
                const int32 Seed = PCGHelpers::ComputeSeed(Context->GetSeed(), BlockID);
                FRandomStream Random(Seed);
                TArray<CityPacking::FPlacement> Best;
                double BestArea = -1;
                for (int32 Attempt = 0; Attempt < FMath::Clamp(Settings->LayoutAttempts, 1, 64); ++Attempt)
                {
                    auto Trial = CityPacking::Pack(Types, W, H, Gap, FMath::Clamp(Settings->MaxBuildingsPerBlock, 1, 64),
                        Random, Settings->bAllowQuarterTurns, &Weights);
                    double Area = 0;
                    for (const auto& P : Trial) Area += (P.Rect.W - Gap) * (P.Rect.H - Gap);
                    if (Area > BestArea) { BestArea = Area; Best = MoveTemp(Trial); }
                }
                const double Coverage = 100 * FMath::Max(0.0, BestArea) / (W * H);
                for (int32 LotID = 0; LotID < Best.Num(); ++LotID)
                {
                    const auto& P = Best[LotID];
                    const FCatalogEntry& Entry = Catalog[P.Type];
                    FPCGPoint& Point = Placements->GetMutablePoints().AddDefaulted_GetRef();
                    Point.Transform = PlacementTransform(Frame, Min, Max.Z, Entry, P, Gap, Inset);
                    Point.BoundsMin = Entry.Footprint.BoundsMin;
                    Point.BoundsMax = Entry.Footprint.BoundsMax;
                    Point.Seed = PCGHelpers::ComputeSeed(Seed, LotID);
                    Point.Density = 1;
                    Point.MetadataEntry = Placements->Metadata->AddEntry();
                    PlacementAttributes.Set(Point.MetadataEntry, Entry, BlockID, LotID, Seed, P.Quarter, Coverage);

                    FPCGPoint& Area = Footprints->GetMutablePoints().AddDefaulted_GetRef();
                    Area.Transform = FTransform(Frame.GetRotation(), Frame.TransformPosition(FVector(
                        Min.X + Inset + P.Rect.X + (P.Rect.W - Gap) * 0.5,
                        Min.Y + Inset + P.Rect.Y + (P.Rect.H - Gap) * 0.5, Max.Z)), FVector::OneVector);
                    Area.BoundsMin = FVector(-(P.Rect.W - Gap) * 0.5, -(P.Rect.H - Gap) * 0.5, 0);
                    Area.BoundsMax = FVector((P.Rect.W - Gap) * 0.5, (P.Rect.H - Gap) * 0.5, 20);
                    Area.Seed = Point.Seed;
                    Area.Density = 1;
                    Area.Color = FLinearColor::MakeFromHSV8(static_cast<uint8>(P.Type * 83), 180, 255);
                    Area.MetadataEntry = Footprints->Metadata->AddEntry();
                    FootprintAttributes.Set(Area.MetadataEntry, Entry, BlockID, LotID, Seed, P.Quarter, Coverage);
                }
            }
        }
        FPCGTaggedData& PlacementOutput = Context->OutputData.TaggedData.Emplace_GetRef();
        PlacementOutput.Data = Placements;
        PlacementOutput.Pin = PCGPinConstants::DefaultOutputLabel;
        FPCGTaggedData& FootprintOutput = Context->OutputData.TaggedData.Emplace_GetRef();
        FootprintOutput.Data = Footprints;
        FootprintOutput.Pin = FootprintsPin;
        UE_LOG(LogTemp, Display, TEXT("CityBlockPacking: blocks=%d buildings=%d gap=%.1f rejectedRows=%d"),
            BlockCount, Placements->GetNumPoints(), Gap, RejectedRows);
        return true;
    }
};

TArray<FPCGPinProperties> UCityBlockPackingSettings::InputPinProperties() const
{
    TArray<FPCGPinProperties> Pins;
    Pins.Emplace_GetRef(CityBlockPacking::BlocksPin, EPCGDataType::Point).SetRequiredPin();
    Pins.Emplace_GetRef(CityBlockPacking::BuildingsPin, EPCGDataType::Param).SetRequiredPin();
    return Pins;
}

TArray<FPCGPinProperties> UCityBlockPackingSettings::OutputPinProperties() const
{
    TArray<FPCGPinProperties> Pins = DefaultPointOutputPinProperties();
    Pins.Emplace(CityBlockPacking::FootprintsPin, EPCGDataType::Point);
    return Pins;
}

FPCGElementPtr UCityBlockPackingSettings::CreateElement() const
{
    return MakeShared<FCityBlockPackingElement>();
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityBlockPackingTransformTest, "Project.PCG.CityBlockPacking.RotatedPivotAndGround",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityBlockPackingTransformTest::RunTest(const FString&)
{
    CityBlockPacking::FCatalogEntry Entry;
    Entry.Footprint.BoundsMin = FVector(-1824.346868, -773.939972, -228);
    Entry.Footprint.BoundsMax = FVector(374.388173, 1599.168491, 2438.484117);
    Entry.GroundOffsetZ = 45;
    const FTransform Frame(FRotator(0, 35, 0), FVector(1200, -800, 125), FVector::OneVector);
    const FVector Center = (Entry.Footprint.BoundsMin + Entry.Footprint.BoundsMax) * 0.5;
    for (int32 Quarter = 0; Quarter < 4; ++Quarter)
    {
        const FVector Size = Entry.Footprint.BoundsMax - Entry.Footprint.BoundsMin;
        const double W = Quarter % 2 ? Size.Y : Size.X;
        const double H = Quarter % 2 ? Size.X : Size.Y;
        CityPacking::FPlacement Placement{{100, 200, W + 250, H + 250}, 0, Quarter};
        const auto Transform = CityBlockPacking::PlacementTransform(Frame, FVector(-3250, -3250, -800), 0, Entry, Placement, 250, 0);
        const FVector ExpectedCenter = Frame.TransformPosition(FVector(-3150 + W * 0.5, -3050 + H * 0.5, 0));
        // 로컬 접지면(-GroundOffsetZ)의 XY 경계 중심이 목표 표면에 도달하는지 검증
        const FVector ActualCenter = Transform.TransformPosition(FVector(Center.X, Center.Y, -Entry.GroundOffsetZ));
        TestTrue(TEXT("Rotated asymmetric pivot keeps contact center on block surface"), ActualCenter.Equals(ExpectedCenter, 0.001));
        FVector Min(MAX_dbl), Max(-MAX_dbl);
        for (int32 X = 0; X < 2; ++X)
            for (int32 Y = 0; Y < 2; ++Y)
            {
                const FVector LocalCorner = Frame.InverseTransformPosition(Transform.TransformPosition(FVector(
                    X ? Entry.Footprint.BoundsMax.X : Entry.Footprint.BoundsMin.X,
                    Y ? Entry.Footprint.BoundsMax.Y : Entry.Footprint.BoundsMin.Y, -Entry.GroundOffsetZ)));
                Min = Min.ComponentMin(LocalCorner);
                Max = Max.ComponentMax(LocalCorner);
            }
        TestTrue(TEXT("Rotated footprint starts at packed rectangle"), FVector2D(Min.X, Min.Y).Equals(FVector2D(-3150, -3050), 0.001));
        TestTrue(TEXT("Rotated footprint ends at packed rectangle"), FVector2D(Max.X, Max.Y).Equals(FVector2D(-3150 + W, -3050 + H), 0.001));
    }
    return true;
}
#endif
