#include "AI/TBPatrolPoint.h"
#include "Components/BillboardComponent.h"
#include "AI/TBPatrolPointSubsystem.h"
#include "NavigationData.h"

void ATBPatrolPoint::ClearPatrolLinks() { PatrolLinks.Empty(); }

void ATBPatrolPoint::AddPatrolLink(const ANavigationData* NavData, ATBPatrolPoint* Point)
{
	if (NavData && IsValid(Point) && Point != this) PatrolLinks.FindOrAdd(NavData).AddUnique(Point);
}

ATBPatrolPoint* ATBPatrolPoint::ChooseNextPoint(const ANavigationData* NavData, const ATBPatrolPoint* Previous) const
{
	const auto* Links = PatrolLinks.Find(NavData);
	if (!Links) return nullptr;
	TArray<ATBPatrolPoint*> Choices;
	for (const auto& Link : *Links) if (Link.IsValid() && Link.Get() != Previous) Choices.Add(Link.Get());
	if (Choices.IsEmpty()) for (const auto& Link : *Links) if (Link.IsValid()) Choices.Add(Link.Get());
	return Choices.IsEmpty() ? nullptr : Choices[FMath::RandRange(0, Choices.Num() - 1)];
}

ATBPatrolPoint::ATBPatrolPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	
	USceneComponent* Root = GetRootComponent();
	
#if WITH_EDITORONLY_DATA
	EditorIcon = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("EditorIcon"));
	
	if (EditorIcon)
	{
		EditorIcon->SetupAttachment(Root);
	}
#endif		
}

void ATBPatrolPoint::BeginPlay()
{
	Super::BeginPlay();
	
	if (UTBPatrolPointSubsystem* Subsystem = GetWorld()->GetSubsystem<UTBPatrolPointSubsystem>())
	{
		Subsystem->RegisterPatrolPoint(this);
	}
}

void ATBPatrolPoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UTBPatrolPointSubsystem* Subsystem = GetWorld()->GetSubsystem<UTBPatrolPointSubsystem>())
	{
		Subsystem->UnregisterPatrolPoint(this);
	}
		
	Super::EndPlay(EndPlayReason);
}
