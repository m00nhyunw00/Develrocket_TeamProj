#include "PCG/CityLayoutCoordinator.h"
#include "PCG/CityNavMeshComponent.h"

#include "PCGComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCityLayout, Log, All);

ACityLayoutCoordinator::ACityLayoutCoordinator()
{
    bReplicates = true;
    bAlwaysRelevant = true;
    PrimaryActorTick.bCanEverTick = false;
    CityNavMesh = CreateDefaultSubobject<UCityNavMeshComponent>(TEXT("CityNavMesh"));
}

void ACityLayoutCoordinator::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACityLayoutCoordinator, Layout);
}

void ACityLayoutCoordinator::BeginPlay()
{
    Super::BeginPlay();
    CityPCG = CityVolume ? CityVolume->FindComponentByClass<UPCGComponent>() : nullptr;
    if (!CityPCG)
    {
        UE_LOG(LogCityLayout, Error, TEXT("CityVolume or its PCG component is missing on %s"), *GetName());
        return;
    }
    CityPCG->OnPCGGraphGeneratedExternal.AddDynamic(this, &ACityLayoutCoordinator::OnGenerated);
    CityPCG->OnPCGGraphCancelledExternal.AddDynamic(this, &ACityLayoutCoordinator::OnCancelled);
    CityPCG->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
    CityPCG->CleanupLocalImmediate(true);
    bInitialized = true;
    if (HasAuthority()) GenerateNewCity();
    else ApplyLatestLayout();
}

void ACityLayoutCoordinator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bInitialized = false;
    if (CityPCG)
    {
        CityPCG->OnPCGGraphGeneratedExternal.RemoveDynamic(this, &ACityLayoutCoordinator::OnGenerated);
        CityPCG->OnPCGGraphCancelledExternal.RemoveDynamic(this, &ACityLayoutCoordinator::OnCancelled);
    }
    Super::EndPlay(EndPlayReason);
}

void ACityLayoutCoordinator::GenerateNewCity()
{
    if (!HasAuthority() || !bInitialized) return;
    Layout.Seed = static_cast<int32>(GetTypeHash(FGuid::NewGuid()) & 0x7fffffff);
    ++Layout.Revision;
    ForceNetUpdate();
    ApplyLatestLayout();
}

void ACityLayoutCoordinator::OnRep_Layout()
{
    ApplyLatestLayout();
}

void ACityLayoutCoordinator::ApplyLatestLayout()
{
    if (!bInitialized || !CityPCG || Layout.Revision <= 0 || CompletedRevision == Layout.Revision) return;
    bLocalCityReady = false;
    if (HasAuthority() && CityNavMesh) CityNavMesh->InvalidateCityNavigation();
    if (CityPCG->IsGenerating() || CityPCG->IsCleaningUp()) return;
    CityPCG->CleanupLocalImmediate(true);
    CityPCG->Seed = Layout.Seed;
    GeneratingRevision = Layout.Revision;
    UE_LOG(LogCityLayout, Display, TEXT("Generating world=%s role=%d seed=%d revision=%d"),
        *GetWorld()->GetName(), static_cast<int32>(GetLocalRole()), Layout.Seed, Layout.Revision);
    CityPCG->GenerateLocal(true);
}

void ACityLayoutCoordinator::OnGenerated(UPCGComponent* Component)
{
    if (Component != CityPCG || !bInitialized) return;
    if (GeneratingRevision != Layout.Revision)
    {
        GetWorldTimerManager().SetTimerForNextTick(this, &ACityLayoutCoordinator::ApplyLatestLayout);
        return;
    }
    CompletedRevision = GeneratingRevision;
    bLocalCityReady = true;
    UE_LOG(LogCityLayout, Display, TEXT("Ready world=%s role=%d seed=%d revision=%d"),
        *GetWorld()->GetName(), static_cast<int32>(GetLocalRole()), Layout.Seed, Layout.Revision);
    if (HasAuthority() && CityNavMesh) CityNavMesh->RequestCityNavigationUpdate(CityVolume, Layout.Revision);
    OnLocalCityReady.Broadcast(Layout.Seed, Layout.Revision);
}

void ACityLayoutCoordinator::OnCancelled(UPCGComponent* Component)
{
    if (Component == CityPCG && bInitialized && CompletedRevision != Layout.Revision)
        GetWorldTimerManager().SetTimerForNextTick(this, &ACityLayoutCoordinator::ApplyLatestLayout);
}
