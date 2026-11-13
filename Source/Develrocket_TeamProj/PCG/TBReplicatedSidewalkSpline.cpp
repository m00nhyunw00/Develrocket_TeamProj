#include "PCG/TBReplicatedSidewalkSpline.h"

#include "Components/SplineMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ATBReplicatedSidewalkSpline::ATBReplicatedSidewalkSpline()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);
	PrimaryActorTick.bCanEverTick = false;
	SplineMesh = CreateDefaultSubobject<USplineMeshComponent>(TEXT("WalkableSpline"));
	SetRootComponent(SplineMesh);
	SplineMesh->SetMobility(EComponentMobility::Movable);
	SplineMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SplineMesh->SetCollisionObjectType(ECC_WorldStatic);
	SplineMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	SplineMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	SplineMesh->SetCanEverAffectNavigation(false);
}

void ATBReplicatedSidewalkSpline::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATBReplicatedSidewalkSpline, Shape);
}

void ATBReplicatedSidewalkSpline::OnRep_Shape()
{
	if (!IsValid(Shape.Mesh)) return;
	SplineMesh->SetStaticMesh(Shape.Mesh);
	SplineMesh->SetForwardAxis(static_cast<ESplineMeshAxis::Type>(Shape.ForwardAxis), false);
	SplineMesh->SetStartScale(Shape.StartScale, false);
	SplineMesh->SetEndScale(Shape.EndScale, false);
	SplineMesh->SetStartAndEnd(Shape.Start, Shape.StartTangent, Shape.End, Shape.EndTangent, true);
}

void ATBReplicatedSidewalkSpline::ReplaceWithReplicatedSpline(AActor* Sidewalk, USplineMeshComponent* LocalSpline)
{
	UWorld* World = IsValid(Sidewalk) ? Sidewalk->GetWorld() : nullptr;
	if (!World || !World->IsGameWorld() || !IsValid(LocalSpline) || LocalSpline->GetOwner() != Sidewalk) return;
	const TWeakObjectPtr<AActor> WeakSidewalk(Sidewalk);
	const TWeakObjectPtr<USplineMeshComponent> WeakSpline(LocalSpline);
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakSidewalk, WeakSpline]()
	{
		AActor* Sidewalk = WeakSidewalk.Get();
		USplineMeshComponent* LocalSpline = WeakSpline.Get();
		UWorld* World = IsValid(Sidewalk) ? Sidewalk->GetWorld() : nullptr;
		if (!World || !IsValid(LocalSpline) || LocalSpline->GetOwner() != Sidewalk) return;
		if (World->GetNetMode() != NM_Client)
		{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ATBReplicatedSidewalkSpline* Replica = World->SpawnActor<ATBReplicatedSidewalkSpline>(StaticClass(), LocalSpline->GetComponentTransform(), Params);
		if (!Replica) return;
		Replica->SourceSidewalk = Sidewalk;
		Sidewalk->OnDestroyed.AddUniqueDynamic(Replica, &ThisClass::HandleSidewalkDestroyed);
		Replica->Shape.Mesh = LocalSpline->GetStaticMesh();
		Replica->Shape.Start = LocalSpline->GetStartPosition();
		Replica->Shape.StartTangent = LocalSpline->GetStartTangent();
		Replica->Shape.End = LocalSpline->GetEndPosition();
		Replica->Shape.EndTangent = LocalSpline->GetEndTangent();
		Replica->Shape.StartScale = LocalSpline->GetStartScale();
		Replica->Shape.EndScale = LocalSpline->GetEndScale();
		Replica->Shape.ForwardAxis = static_cast<uint8>(LocalSpline->GetForwardAxis());
		Replica->OnRep_Shape();
		Replica->ForceNetUpdate();
		}
		LocalSpline->DestroyComponent();
	}));
}

void ATBReplicatedSidewalkSpline::ClearForSidewalk(AActor* Sidewalk)
{
	UWorld* World = IsValid(Sidewalk) ? Sidewalk->GetWorld() : nullptr;
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client) return;
	for (TActorIterator<ATBReplicatedSidewalkSpline> It(World); It; ++It)
	{
		if (It->SourceSidewalk.Get() == Sidewalk) It->Destroy();
	}
}

void ATBReplicatedSidewalkSpline::HandleSidewalkDestroyed(AActor*)
{
	if (HasAuthority()) Destroy();
}

void ATBReplicatedSidewalkSpline::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AActor* Sidewalk = SourceSidewalk.Get()) Sidewalk->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleSidewalkDestroyed);
	Super::EndPlay(EndPlayReason);
}
