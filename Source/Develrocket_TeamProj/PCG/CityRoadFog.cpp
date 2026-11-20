#include "PCG/CityLayoutCoordinator.h"

#include "Components/LocalFogVolumeComponent.h"
#include "Engine/World.h"
#include "PCGComponent.h"
#include "PCGManagedResource.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogCityRoadFog, Log, All);

namespace
{
    struct FSidewalkBlock
    {
        FBox2D Bounds;
        double Elevation;
    };

    struct FFogSite
    {
        FVector Location;
        bool bIntersection;
        uint32 Rank;
    };

    bool ReadSidewalkSpan(const AActor& Sidewalk, FName Name, double& Span)
    {
        const FNumericProperty* Property = FindFProperty<FNumericProperty>(Sidewalk.GetClass(), Name);
        if (!Property || !Property->IsFloatingPoint()) return false;
        Span = Property->GetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<void>(&Sidewalk));
        return FMath::IsFinite(Span) && Span > 0.0;
    }

    TArray<FSidewalkBlock> CollectSidewalkBlocks(const UPCGComponent& City)
    {
        static const FName SidewalkTag(TEXT("PCG_CitySidewalk"));
        static const FName SidewalkClassName(TEXT("BP_CitySidewalk_Assembly_C"));
        TArray<FSidewalkBlock> Blocks;
        City.ForEachConstManagedResource([&Blocks](const UPCGManagedResource* Resource)
        {
            const UPCGManagedActors* Actors = Cast<UPCGManagedActors>(Resource);
            if (!Actors) return;

            for (const TSoftObjectPtr<AActor>& ActorRef : Actors->GetConstGeneratedActors())
            {
                const AActor* Sidewalk = ActorRef.Get();
                if (!IsValid(Sidewalk) || Sidewalk->IsActorBeingDestroyed()) continue;
                if (!Sidewalk->ActorHasTag(SidewalkTag) && Sidewalk->GetClass()->GetFName() != SidewalkClassName) continue;

                double SpanX, SpanY;
                if (!ReadSidewalkSpan(*Sidewalk, TEXT("CornerSpanX"), SpanX) ||
                    !ReadSidewalkSpan(*Sidewalk, TEXT("CornerSpanY"), SpanY)) continue;

                const FTransform& Frame = Sidewalk->GetActorTransform();
                FBox2D Bounds(ForceInit);
                for (double X : {-0.5 * SpanX, 0.5 * SpanX})
                {
                    for (double Y : {-0.5 * SpanY, 0.5 * SpanY})
                        Bounds += FVector2D(Frame.TransformPositionNoScale(FVector(X, Y, 0.0)));
                }
                Blocks.Add({Bounds, Frame.GetLocation().Z});
            }
        });
        return Blocks;
    }

    TArray<FFogSite> FindRoadFogSites(const TArray<FSidewalkBlock>& Blocks, int32 Seed)
    {
        constexpr double MinRoadGap = 200.0;
        constexpr double MaxRoadGap = 3500.0;
        constexpr double MinSharedFrontage = 2000.0;
        TArray<FFogSite> Sites;
        for (int32 Index = 0; Index < Blocks.Num(); ++Index)
        {
            const FSidewalkBlock& A = Blocks[Index];
            for (int32 OtherIndex = Index + 1; OtherIndex < Blocks.Num(); ++OtherIndex)
            {
                const FSidewalkBlock& B = Blocks[OtherIndex];
                const FVector2D Min = A.Bounds.Min.ComponentMax(B.Bounds.Min);
                const FVector2D Max = A.Bounds.Max.ComponentMin(B.Bounds.Max);
                const FVector2D Gap = Min - Max;
                const bool bRoadGapX = Gap.X >= MinRoadGap && Gap.X <= MaxRoadGap;
                const bool bRoadGapY = Gap.Y >= MinRoadGap && Gap.Y <= MaxRoadGap;
                const bool bIntersection = bRoadGapX && bRoadGapY;
                const bool bStreet = (bRoadGapX && Gap.Y <= -MinSharedFrontage) ||
                                     (bRoadGapY && Gap.X <= -MinSharedFrontage);
                if (!bIntersection && !bStreet) continue;

                // Diagonal blocks locate a crossing. Facing blocks locate the center of a road segment.
                const FVector2D Center = (Min + Max) * 0.5;
                if (Blocks.ContainsByPredicate([&Center](const FSidewalkBlock& Block)
                    { return Block.Bounds.IsInside(Center); })) continue;

                const double Elevation = FMath::Min(A.Elevation, B.Elevation);
                const uint32 PositionHash = HashCombine(GetTypeHash(FMath::RoundToInt64(Center.X)),
                    GetTypeHash(FMath::RoundToInt64(Center.Y)));
                Sites.Add({FVector(Center.X, Center.Y, Elevation), bIntersection,
                    HashCombine(GetTypeHash(Seed), PositionHash)});
            }
        }

        Sites.Sort([](const FFogSite& A, const FFogSite& B)
        {
            if (A.bIntersection != B.bIntersection) return A.bIntersection;
            if (A.Rank != B.Rank) return A.Rank < B.Rank;
            if (A.Location.X != B.Location.X) return A.Location.X < B.Location.X;
            if (A.Location.Y != B.Location.Y) return A.Location.Y < B.Location.Y;
            return A.Location.Z < B.Location.Z;
        });
        return Sites;
    }
}

void ACityLayoutCoordinator::BuildRoadFog()
{
    ClearRoadFog();
    UWorld* World = GetWorld();
    const int32 VolumeLimit = FMath::Clamp(MaxRoadFogVolumes, 0, 32);
    if (!World || World->GetNetMode() == NM_DedicatedServer || !CityPCG ||
        VolumeLimit == 0 || RoadFogDensity <= 0.0f) return;

    const TArray<FSidewalkBlock> Blocks = CollectSidewalkBlocks(*CityPCG);
    const TArray<FFogSite> Sites = FindRoadFogSites(Blocks, Layout.Seed);
    for (const FFogSite& Site : Sites)
    {
        if (RoadFogVolumes.Num() >= VolumeLimit) break;
        const float Radius = FMath::Max(500.0f, RoadFogRadius) * (Site.bIntersection ? 1.15f : 1.0f);
        const FVector Center = Site.Location + FVector(0.0, 0.0, 100.0);
        const double MinSpacingSquared = FMath::Square(1.6 * Radius);
        if (RoadFogVolumes.ContainsByPredicate([&Center, MinSpacingSquared](const ULocalFogVolumeComponent* Fog)
            { return FVector::DistSquared2D(Fog->GetComponentLocation(), Center) < MinSpacingSquared; })) continue;

        ULocalFogVolumeComponent* Fog = NewObject<ULocalFogVolumeComponent>(this, NAME_None, RF_Transient);
        AddInstanceComponent(Fog);
        Fog->SetWorldLocation(Center);
        Fog->SetWorldScale3D(FVector(Radius / ULocalFogVolumeComponent::GetBaseVolumeSize()));
        Fog->SetRadialFogExtinction(RoadFogDensity * 0.8f);
        Fog->SetHeightFogExtinction(RoadFogDensity);
        Fog->SetHeightFogFalloff(1100.0f);
        Fog->SetFogAlbedo(FLinearColor(0.67f, 0.76f, 0.80f));
        // Keep the mist visible in streets where the local fog receives little light.
        Fog->SetFogEmissive(FLinearColor(0.085f, 0.105f, 0.12f));
        Fog->RegisterComponent();
        RoadFogVolumes.Add(Fog);
    }
    UE_LOG(LogCityRoadFog, Display, TEXT("Revision %d: placed %d road fog volumes from %d sidewalk blocks (%d candidates)."),
        Layout.Revision, RoadFogVolumes.Num(), Blocks.Num(), Sites.Num());
}

void ACityLayoutCoordinator::ClearRoadFog()
{
    for (ULocalFogVolumeComponent* Fog : RoadFogVolumes)
    {
        if (IsValid(Fog)) Fog->DestroyComponent();
    }
    RoadFogVolumes.Reset();
}
