#include "PCG/CityPatrolCluster.h"

#include "Data/PCGBasePointData.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "PCGElement.h"

namespace CityPatrolCluster
{
    TArray<FVector> BuildCenters(TArray<FVector> Points, double Distance)
    {
        Points.Sort([](const FVector& A, const FVector& B)
        {
            if (A.X != B.X) return A.X < B.X;
            if (A.Y != B.Y) return A.Y < B.Y;
            return A.Z < B.Z;
        });

        TArray<bool> Visited;
        Visited.Init(false, Points.Num());
        TArray<FVector> Centers;
        const double DistanceSquared = FMath::Square(FMath::Max(0.0, Distance));
        for (int32 Start = 0; Start < Points.Num(); ++Start)
        {
            if (Visited[Start]) continue;
            TArray<int32> Pending{Start};
            Visited[Start] = true;
            FVector Sum = FVector::ZeroVector;
            for (int32 Cursor = 0; Cursor < Pending.Num(); ++Cursor)
            {
                const FVector& Current = Points[Pending[Cursor]];
                Sum += Current;
                for (int32 Candidate = 0; Candidate < Points.Num(); ++Candidate)
                {
                    if (!Visited[Candidate] && FVector::DistSquared2D(Current, Points[Candidate]) <= DistanceSquared)
                    {
                        Visited[Candidate] = true;
                        Pending.Add(Candidate);
                    }
                }
            }
            Centers.Add(Sum / static_cast<double>(Pending.Num()));
        }
        return Centers;
    }
}

class FCityPatrolClusterElement : public IPCGElement
{
protected:
    virtual bool SupportsBasePointDataInputs(FPCGContext*) const override { return true; }
    virtual bool ExecuteInternal(FPCGContext* Context) const override
    {
        const auto* Settings = Context->GetInputSettings<UCityPatrolClusterSettings>();
        TArray<FVector> Locations;
        for (const FPCGTaggedData& Tagged : Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel))
        {
            if (const UPCGBasePointData* Data = Cast<UPCGBasePointData>(Tagged.Data))
            {
                for (int32 Index = 0; Index < Data->GetNumPoints(); ++Index)
                {
                    Locations.Add(Data->GetTransform(Index).GetLocation());
                }
            }
        }

        UPCGPointData* Output = NewObject<UPCGPointData>();
        for (const FVector& Center : CityPatrolCluster::BuildCenters(MoveTemp(Locations), Settings->MergeDistance))
        {
            FPCGPoint& Point = Output->GetMutablePoints().AddDefaulted_GetRef();
            Point.Transform = FTransform(Center);
            Point.Density = 1.0f;
        }
        FPCGTaggedData& TaggedOutput = Context->OutputData.TaggedData.Emplace_GetRef();
        TaggedOutput.Data = Output;
        TaggedOutput.Pin = PCGPinConstants::DefaultOutputLabel;
        return true;
    }
};

FPCGElementPtr UCityPatrolClusterSettings::CreateElement() const
{
    return MakeShared<FCityPatrolClusterElement>();
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityPatrolClusterTest, "Project.PCG.CityPatrol.ClusterCenters",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityPatrolClusterTest::RunTest(const FString& Parameters)
{
    const TArray<FVector> Centers = CityPatrolCluster::BuildCenters(
        {FVector(0, 0, 0), FVector(300, 0, 0), FVector(0, 300, 0), FVector(300, 300, 0), FVector(2000, 0, 0)}, 600.0);
    TestEqual(TEXT("Nearby points form one group and isolated point remains"), Centers.Num(), 2);
    if (Centers.Num() == 2)
    {
        TestTrue(TEXT("Group center is the arithmetic mean"), Centers[0].Equals(FVector(150, 150, 0)));
        TestTrue(TEXT("Isolated point is retained"), Centers[1].Equals(FVector(2000, 0, 0)));
    }
    return true;
}
#endif
