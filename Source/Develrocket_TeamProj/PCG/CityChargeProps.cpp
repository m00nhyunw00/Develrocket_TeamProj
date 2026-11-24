#include "PCG/CityLayoutCoordinator.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "UObject/Package.h"

namespace
{
const FName BollardName(TEXT("sm_bollard_a"));
const FName StreetlightName(TEXT("sm_streetlight_a"));
const FName BollardPackage(TEXT("/Game/_Assets/_Original/VictorianStreet/Meshes/Static/sm_bollard_a"));
const FName StreetlightPackage(TEXT("/Game/_Assets/_Original/VictorianStreet/Meshes/Static/sm_streetlight_a"));

FName GetChargePropName(const UStaticMeshComponent* Component)
{
    const UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
    if (!Mesh) return NAME_None;

    const FName Name = Mesh->GetFName();
    if (Name != BollardName && Name != StreetlightName) return NAME_None;

    const FName ExpectedPackage = Name == BollardName ? BollardPackage : StreetlightPackage;
    return Mesh->GetOutermost()->GetFName() == ExpectedPackage ? Name : NAME_None;
}

bool RemoveProp(UStaticMeshComponent& Component, int32 InstanceIndex)
{
    if (auto* Instances = Cast<UInstancedStaticMeshComponent>(&Component))
    {
        return Instances->RemoveInstance(InstanceIndex);
    }

    Component.SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component.SetVisibility(false, true);
    return true;
}

void DisableStreetlight(UWorld& World, const FVector& PropLocation)
{
    static const FSoftClassPath LightClassPath(TEXT("/Game/_Assets/_Original/DayNightCycle/Blueprints/BP_Lights.BP_Lights_C"));
    UClass* LightClass = LightClassPath.ResolveClass();
    if (!LightClass) return;

    // The separate light actor can sit above the mesh pivot. Match the placement in XY.
    AActor* MatchingLight = nullptr;
    double BestDistance = TNumericLimits<double>::Max();
    for (TActorIterator<AActor> It(&World, LightClass); It; ++It)
    {
        const FVector LightLocation = It->GetActorLocation();
        if (FVector::DistSquared2D(LightLocation, PropLocation) > FMath::Square(5.0)
            || FMath::Abs(LightLocation.Z - PropLocation.Z) > 1000.0) continue;

        const double Distance = FVector::DistSquared(LightLocation, PropLocation);
        if (Distance < BestDistance)
        {
            MatchingLight = *It;
            BestDistance = Distance;
        }
    }
    if (!MatchingLight) return;

    TInlineComponentArray<ULightComponent*> Lights(MatchingLight);
    for (ULightComponent* Light : Lights)
    {
        Light->SetVisibility(false);
        Light->SetHiddenInGame(true);
    }
}

void ApplyDestroyedProp(UWorld& World, const FDestroyedCityProp& Prop,
    const TArray<TWeakObjectPtr<UStaticMeshComponent>>& PropComponents)
{
    UStaticMeshComponent* MatchingComponent = nullptr;
    int32 MatchingIndex = INDEX_NONE;
    double BestDistanceSquared = FMath::Square(1.0);
    const auto ConsiderPlacement = [&](UStaticMeshComponent& Component, int32 Index, const FVector& Location)
    {
        const double DistanceSquared = FVector::DistSquared(Location, Prop.Location);
        if (DistanceSquared < BestDistanceSquared)
        {
            MatchingComponent = &Component;
            MatchingIndex = Index;
            BestDistanceSquared = DistanceSquared;
        }
    };

    // Resolve the placement against the current local instance indices after every removal.
    for (const auto& Entry : PropComponents)
    {
        UStaticMeshComponent* Component = Entry.Get();
        if (GetChargePropName(Component) != Prop.MeshName) continue;

        if (auto* Instances = Cast<UInstancedStaticMeshComponent>(Component))
        {
            for (int32 Index = 0; Index < Instances->GetInstanceCount(); ++Index)
            {
                FTransform Transform;
                if (Instances->GetInstanceTransform(Index, Transform, true))
                {
                    ConsiderPlacement(*Instances, Index, Transform.GetLocation());
                }
            }
        }
        else if (Component->IsVisible())
        {
            ConsiderPlacement(*Component, INDEX_NONE, Component->GetComponentLocation());
        }
    }

    if (MatchingComponent) RemoveProp(*MatchingComponent, MatchingIndex);
    if (Prop.MeshName == StreetlightName) DisableStreetlight(World, Prop.Location);
}
}

bool ACityLayoutCoordinator::IsChargeBreakableProp(const UPrimitiveComponent* Component)
{
    return !GetChargePropName(Cast<UStaticMeshComponent>(Component)).IsNone();
}

void ACityLayoutCoordinator::CacheChargePropComponents()
{
    ChargePropComponents.Reset();
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        TInlineComponentArray<UStaticMeshComponent*> Components(*It);
        for (UStaticMeshComponent* Component : Components)
        {
            if (IsValid(Component) && !GetChargePropName(Component).IsNone())
            {
                ChargePropComponents.Add(Component);
            }
        }
    }
}

bool ACityLayoutCoordinator::BreakChargePropsAlongPath(const FVector& Start, const FVector& End, const FVector& CapsuleExtent)
{
    if (!HasAuthority() || !bLocalCityReady || CompletedRevision != DestroyedProps.Revision) return false;

    const FVector Direction = End - Start;
    // Include the separation left by CharacterMovement when it stops at a blocking surface.
    const FVector Extent = CapsuleExtent + FVector(1.0);
    const auto IntersectsPath = [&](const FBox& Bounds)
    {
        const FBox Expanded = Bounds.ExpandBy(Extent);
        return Expanded.IsInsideOrOn(Start) || FMath::LineBoxIntersection(Expanded, Start, End, Direction);
    };

    bool bBrokeProp = false;
    for (const auto& Entry : ChargePropComponents)
    {
        UStaticMeshComponent* Component = Entry.Get();
        if (!Component || !Component->IsVisible() || !IntersectsPath(Component->Bounds.GetBox())) continue;

        if (auto* Instances = Cast<UInstancedStaticMeshComponent>(Component))
        {
            const UStaticMesh* Mesh = Instances->GetStaticMesh();
            if (!Mesh) continue;
            const FBox LocalBounds = Mesh->GetBoundingBox();
            // Work backwards so both shifting and swap removal preserve the remaining indices.
            for (int32 Index = Instances->GetInstanceCount() - 1; Index >= 0; --Index)
            {
                FTransform Transform;
                if (Instances->GetInstanceTransform(Index, Transform, true) && IntersectsPath(LocalBounds.TransformBy(Transform)))
                {
                    bBrokeProp |= BreakChargeProp(*Instances, Index, Transform.GetLocation());
                }
            }
        }
        else
        {
            bBrokeProp |= BreakChargeProp(*Component, INDEX_NONE, Component->GetComponentLocation());
        }
    }
    return bBrokeProp;
}

bool ACityLayoutCoordinator::BreakChargeProp(UStaticMeshComponent& Component, int32 InstanceIndex, const FVector& Location)
{
    const FName MeshName = GetChargePropName(&Component);
    if (MeshName.IsNone() || !RemoveProp(Component, InstanceIndex)) return false;

    if (MeshName == StreetlightName) DisableStreetlight(*GetWorld(), Location);
    FDestroyedCityProp& Prop = DestroyedProps.Props.AddDefaulted_GetRef();
    Prop.MeshName = MeshName;
    Prop.Location = Location;
    ForceNetUpdate();
    return true;
}

void ACityLayoutCoordinator::OnRep_DestroyedProps()
{
    UWorld* World = GetWorld();
    if (HasAuthority() || !World || !bLocalCityReady || CompletedRevision != DestroyedProps.Revision) return;

    for (; AppliedDestroyedPropCount < DestroyedProps.Props.Num(); ++AppliedDestroyedPropCount)
    {
        ApplyDestroyedProp(*World, DestroyedProps.Props[AppliedDestroyedPropCount], ChargePropComponents);
    }
}
