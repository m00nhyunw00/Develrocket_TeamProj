#include "PCG/TBSidewalkCollision.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ATBSidewalkCollision::ATBSidewalkCollision()
{
	bReplicates = true;
	SetReplicateMovement(false);
	PrimaryActorTick.bCanEverTick = false;
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	SetRootComponent(CollisionBox);
	CollisionBox->InitBoxExtent(FVector::ZeroVector);
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionBox->SetCollisionObjectType(ECC_WorldStatic);
	CollisionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	CollisionBox->SetCanEverAffectNavigation(false);
}

void ATBSidewalkCollision::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATBSidewalkCollision, BoxExtent);
}

void ATBSidewalkCollision::OnRep_BoxExtent()
{
	CollisionBox->SetBoxExtent(BoxExtent, true);
}

void ATBSidewalkCollision::CreateForBox(AActor* Sidewalk, UBoxComponent* SourceBox)
{
	UWorld* World = IsValid(Sidewalk) ? Sidewalk->GetWorld() : nullptr;
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client || !IsValid(SourceBox) || SourceBox->GetOwner() != Sidewalk) return;

	// Construction Script 내부에서는 액터 생성을 금지하므로 생성 완료 후 처리
	const TWeakObjectPtr<AActor> WeakSidewalk(Sidewalk);
	const TWeakObjectPtr<UBoxComponent> WeakBox(SourceBox);
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakSidewalk, WeakBox]()
	{
		AActor* CurrentSidewalk = WeakSidewalk.Get();
		UBoxComponent* CurrentBox = WeakBox.Get();
		UWorld* CurrentWorld = IsValid(CurrentSidewalk) ? CurrentSidewalk->GetWorld() : nullptr;
		if (!CurrentWorld || !IsValid(CurrentBox) || CurrentBox->GetOwner() != CurrentSidewalk) return;

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ATBSidewalkCollision* Collision = CurrentWorld->SpawnActor<ATBSidewalkCollision>(StaticClass(), CurrentBox->GetComponentLocation(), CurrentBox->GetComponentRotation(), Params);
		if (!Collision)
		{
			UE_LOG(LogTemp, Error, TEXT("보도 복제 충돌 생성 실패: %s"), *GetNameSafe(CurrentSidewalk));
			return;
		}
		Collision->SourceSidewalk = CurrentSidewalk;
		CurrentSidewalk->OnDestroyed.AddDynamic(Collision, &ThisClass::HandleSidewalkDestroyed);
		Collision->BoxExtent = CurrentBox->GetScaledBoxExtent();
		Collision->OnRep_BoxExtent();
		Collision->ForceNetUpdate();
	}));
}

void ATBSidewalkCollision::ClearForSidewalk(AActor* Sidewalk)
{
	UWorld* World = IsValid(Sidewalk) ? Sidewalk->GetWorld() : nullptr;
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client) return;
	for (TActorIterator<ATBSidewalkCollision> It(World); It; ++It)
	{
		if (It->SourceSidewalk.Get() == Sidewalk) It->Destroy();
	}
}

void ATBSidewalkCollision::HandleSidewalkDestroyed(AActor* DestroyedActor)
{
	if (HasAuthority()) Destroy();
}

void ATBSidewalkCollision::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AActor* Sidewalk = SourceSidewalk.Get()) Sidewalk->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleSidewalkDestroyed);
	Super::EndPlay(EndPlayReason);
}
