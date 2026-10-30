#include "PCG/CityPacking.h"
#include "PCG/CityPackingAlgorithm.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGElement.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/StaticMesh.h"

ACityPackingActor::ACityPackingActor()
{
    bReplicates = true;
    bAlwaysRelevant = true;
    
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    UBoxComponent* Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("CityBounds"));
    Bounds->SetupAttachment(RootComponent);
    Bounds->SetBoxExtent(FVector(10000,10000,5000));
    Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Bounds->SetGenerateOverlapEvents(false);
    Bounds->SetHiddenInGame(true);
    PCGComponent = CreateDefaultSubobject<UPCGComponent>(TEXT("PCGComponent"));
    PCGComponent->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
    PrimaryActorTick.bCanEverTick = false;
}

void ACityPackingActor::NewLayout()
{
    if (!PCGComponent || PCGComponent->IsGenerating()) return;
    if (GetWorld() && GetWorld()->IsGameWorld())
    {
        // 함수 내부의 HasAuthority 검사로 클라이언트 호출은 무시
        GenerateNewLayoutOnServer();
        return;
    }
    Modify();
    LayoutSeed = static_cast<int32>(GetTypeHash(FGuid::NewGuid()) & 0x7fffffff);
    RebuildSameSeed();
}

void ACityPackingActor::RebuildSameSeed()
{
    if (!PCGComponent || PCGComponent->IsGenerating()) return;
    if (Floor)
    {
        FVector Center, Extent;
        Floor->GetActorBounds(false,Center,Extent);
        if (UBoxComponent* Bounds = Cast<UBoxComponent>(GetDefaultSubobjectByName(TEXT("CityBounds"))))
        {
            Bounds->SetWorldLocationAndRotation(Center,FQuat::Identity);
            Bounds->SetBoxExtent(FVector(Extent.X,Extent.Y,FMath::Max(Extent.Z,5000.0)));
        }
    }
    bReuseSeedNextGeneration = true;
    PCGComponent->CleanupLocalImmediate(true);
    PCGComponent->Seed = LayoutSeed;
    PCGComponent->GenerateLocal(true);
}

void ACityPackingActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    DOREPLIFETIME(ACityPackingActor, LayoutState);
}

void ACityPackingActor::GenerateNewLayoutOnServer()
{
    if (!HasAuthority())
    {
        return;
    }
    LayoutState.Seed = static_cast<int32>(GetTypeHash(FGuid::NewGuid()) & 0x7fffffff);
    ++LayoutState.Revision;
    
    ApplyLayoutState();
    ForceNetUpdate();
}

void ACityPackingActor::BeginPlay()
{
    Super::BeginPlay();
    if (HasAuthority())
    {
        GenerateNewLayoutOnServer();
    }
    else
    {
        ApplyLayoutState();
    }
    // if (bRandomizeOnPlay) NewLayout();
}

void ACityPackingActor::OnRep_LayoutState()
{
    ApplyLayoutState();
}

void ACityPackingActor::ApplyLayoutState()
{
    if (!HasActorBegunPlay() || LayoutState.Revision == 0) return;
    LayoutSeed = LayoutState.Seed;
    
    RebuildSameSeed();
}

class FCityPackingElement : public IPCGElement
{
public:
    virtual bool IsCacheable(const UPCGSettings*) const override { return false; }
    virtual bool CanExecuteOnlyOnMainThread(FPCGContext*) const override { return true; }
protected:
    virtual bool ExecuteInternal(FPCGContext* Context) const override
    {
        const UCityPackingSettings* Settings=Context->GetInputSettings<UCityPackingSettings>();
        UPCGComponent* Component=Cast<UPCGComponent>(Context->ExecutionSource.GetObject());
        ACityPackingActor* Owner=Component?Cast<ACityPackingActor>(Component->GetOwner()):nullptr;
        if (!Owner || !Owner->Floor || Settings->Buildings.IsEmpty())
        {
            UE_LOG(LogTemp,Error,TEXT("City packing requires a CityPackingActor with Floor and building footprints."));
            return true;
        }
        UStaticMeshComponent* FloorMesh=Owner->Floor->FindComponentByClass<UStaticMeshComponent>();
        if (!FloorMesh || !FloorMesh->GetStaticMesh()) return true;
        const FTransform FT=FloorMesh->GetComponentTransform();
        const FVector Scale=FT.GetScale3D();
        const FRotator Rotation=FT.Rotator();
        if (!FMath::IsNearlyZero(Rotation.Pitch,0.01) || !FMath::IsNearlyZero(Rotation.Roll,0.01) || Scale.GetMin()<=0)
        {
            UE_LOG(LogTemp,Error,TEXT("City Floor must be horizontal with positive scale; yaw rotation is supported."));
            return true;
        }
        const FBox Local=FloorMesh->GetStaticMesh()->GetBoundingBox();
        const double Margin=FMath::Max(0.0,Owner->EdgeMargin), Gap=FMath::Max(0.0,Owner->BuildingGap);
        const double W=Local.GetSize().X*Scale.X-2*Margin, H=Local.GetSize().Y*Scale.Y-2*Margin;
        if (W<=0 || H<=0) return true;
        const FVector Corner=FT.TransformPosition(FVector(Local.Min.X,Local.Min.Y,Local.Max.Z));
        const FQuat FloorRotation=FT.GetRotation();
        /*if (!Owner->bReuseSeedNextGeneration)
            Owner->LayoutSeed = static_cast<int32>(GetTypeHash(FGuid::NewGuid()) & 0x7fffffff);*/
        Owner->bReuseSeedNextGeneration = false;
        FRandomStream Random(Owner->LayoutSeed);
        TArray<CityPacking::FPlacement> Best;
        double BestArea=-1;
        for (int32 Attempt=0; Attempt<24; ++Attempt)
        {
            auto Trial=CityPacking::Pack(Settings->Buildings,W,H,Gap,FMath::Clamp(Owner->MaxBuildings,1,256),Random);
            double Area=0;
            for (const auto& P:Trial) Area+=(P.Rect.W-Gap)*(P.Rect.H-Gap);
            if (Area>BestArea) { BestArea=Area; Best=MoveTemp(Trial); }
        }
        UPCGPointData* Data=NewObject<UPCGPointData>();
        auto* ClassAttribute=Data->Metadata->CreateAttribute<FString>(TEXT("BuildingClass"),TEXT(""),false,true);
        auto* TypeAttribute=Data->Metadata->CreateAttribute<int32>(TEXT("BuildingType"),0,false,true);
        TArray<FPCGPoint>& Points=Data->GetMutablePoints();
        const bool FlipX=Random.RandRange(0,1)!=0, FlipY=Random.RandRange(0,1)!=0;
        for (const auto& P:Best)
        {
            const auto& Type=Settings->Buildings[P.Type];
            const double BW=P.Rect.W-Gap, BH=P.Rect.H-Gap;
            const double X=FlipX?W-P.Rect.X-BW:P.Rect.X;
            const double Y=FlipY?H-P.Rect.Y-BH:P.Rect.Y;
            const FQuat Yaw(FVector::UpVector,FMath::DegreesToRadians(P.Quarter*90.0));
            const FVector Center=(Type.BoundsMin+Type.BoundsMax)*0.5;
            const FVector RotatedCenter=Yaw.RotateVector(FVector(Center.X,Center.Y,0));
            const FVector Pivot=FVector(Margin+X+BW*0.5,Margin+Y+BH*0.5,-Type.BoundsMin.Z)-RotatedCenter;
            FPCGPoint& Point=Points.AddDefaulted_GetRef();
            Point.Transform=FTransform(FloorRotation*Yaw,Corner+FloorRotation.RotateVector(Pivot),FVector::OneVector);
            Point.BoundsMin=Type.BoundsMin; Point.BoundsMax=Type.BoundsMax;
            Point.Density=1; Point.Seed=Random.RandHelper(MAX_int32);
            Point.MetadataEntry=Data->Metadata->AddEntry();
            ClassAttribute->SetValue(Point.MetadataEntry,Type.BuildingClass->GetPathName());
            TypeAttribute->SetValue(Point.MetadataEntry,P.Type);
        }
        Owner->LastBuildingCount=Points.Num(); Owner->LastLayoutSeed=Owner->LayoutSeed;
        Owner->LastCoveragePercent=100*BestArea/(W*H);
        FPCGTaggedData& Out=Context->OutputData.TaggedData.Emplace_GetRef();
        Out.Data=Data; Out.Pin=PCGPinConstants::DefaultOutputLabel;
        UE_LOG(LogTemp,Display,TEXT("CityPacking seed=%d buildings=%d gap=%.1f coverage=%.1f%%"),Owner->LayoutSeed,Points.Num(),Gap,Owner->LastCoveragePercent);
        return true;
    }
};

FPCGElementPtr UCityPackingSettings::CreateElement() const { return MakeShared<FCityPackingElement>(); }

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityPackingGeometryTest,"Project.PCG.CityPacking.ClearanceAndBounds",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityPackingGeometryTest::RunTest(const FString& Parameters)
{
    TArray<FCityBuildingFootprint> Types;
    const FVector2D Sizes[]={{3976,3199},{2199,2374},{3270,1710},{1655,3340},{6232,2834},{1216,3655}};
    for (const FVector2D& S:Sizes)
    {
        FCityBuildingFootprint T;
        T.BuildingClass=AActor::StaticClass(); T.BoundsMax=FVector(S.X,S.Y,2500); Types.Add(T);
    }
    for (int32 Seed=0; Seed<100; ++Seed)
    {
        FRandomStream Random(Seed);
        const auto Packed=CityPacking::Pack(Types,19500,19500,250,96,Random);
        TestTrue(TEXT("Dense non-empty result"),Packed.Num()>18);
        TSet<int32> Kinds;
        for (int32 I=0; I<Packed.Num(); ++I)
        {
            const auto& A=Packed[I]; Kinds.Add(A.Type);
            TestTrue(TEXT("Inside Floor with edge margin"),A.Rect.X>=0 && A.Rect.Y>=0 && A.Rect.X+A.Rect.W<=19750.001 && A.Rect.Y+A.Rect.H<=19750.001);
            for (int32 J=I+1; J<Packed.Num(); ++J)
                TestFalse(TEXT("Padded rectangles do not overlap (250cm clearance)"),A.Rect.Intersects(Packed[J].Rect));
        }
        TestEqual(TEXT("All six building types"),Kinds.Num(),6);
        FRandomStream Repeated(Seed);
        const auto Same=CityPacking::Pack(Types,19500,19500,250,96,Repeated);
        TestEqual(TEXT("Fixed seed preserves count"),Packed.Num(),Same.Num());
        for (int32 I=0; I<Packed.Num(); ++I)
            TestTrue(TEXT("Fixed seed preserves placements"),Packed[I].Type==Same[I].Type && Packed[I].Quarter==Same[I].Quarter && Packed[I].Rect.X==Same[I].Rect.X && Packed[I].Rect.Y==Same[I].Rect.Y);
    }
    return true;
}
#endif
