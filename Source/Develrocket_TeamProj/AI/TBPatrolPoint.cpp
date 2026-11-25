#include "AI/TBPatrolPoint.h"
#include "Components/BillboardComponent.h"
#include "AI/TBPatrolPointSubsystem.h"
#include "NavigationData.h"

void ATBPatrolPoint::ClearPatrolLinks() { PatrolLinks.Empty(); }

void ATBPatrolPoint::AddPatrolLink(const ANavigationData* NavData, ATBPatrolPoint* Point)
{
	if (NavData && IsValid(Point) && Point != this) PatrolLinks.FindOrAdd(NavData).AddUnique(Point);
}

bool ATBPatrolPoint::IsJunction() const
{
	for (const auto& Entry : PatrolLinks)
	{
		if (Entry.Value.Num() >= 3) return true;
	}
	return false;
}

ATBPatrolPoint* ATBPatrolPoint::ChooseNextPoint(const ANavigationData* NavData, const ATBPatrolPoint* Previous,
	const FVector& PreferredHeading, const ATBPatrolPoint* TemporarilyBlocked, float PreferredLegLength) const
{
	const auto* Links = PatrolLinks.Find(NavData);
	if (!Links) return nullptr;
	TArray<ATBPatrolPoint*> Choices;
	for (const auto& Link : *Links)
	{
		if (ATBPatrolPoint* Point = Link.Get(); Point && Point != Previous && Point != TemporarilyBlocked) Choices.Add(Point);
	}
	if (Choices.IsEmpty())
	{
		for (const auto& Link : *Links)
		{
			if (ATBPatrolPoint* Point = Link.Get(); Point && Point != TemporarilyBlocked) Choices.Add(Point);
		}
	}
	if (Choices.IsEmpty()) return nullptr;
	if (PreferredHeading.IsNearlyZero() && PreferredLegLength <= 0.0f) return Choices[FMath::RandRange(0, Choices.Num() - 1)];

	const FVector Origin = GetActorLocation();
	TArray<double, TInlineAllocator<8>> CumulativeWeights;
	double TotalWeight = 0.0;
	for (const ATBPatrolPoint* Point : Choices)
	{
		const FVector Offset = Point->GetActorLocation() - Origin;
		const FVector Direction = Offset.GetSafeNormal2D();
		const double Alignment = FVector::DotProduct(PreferredHeading, Direction);
		const double LengthWeight = PreferredLegLength > 0.0f
			? 1.0 + 2.0 / (1.0 + FMath::Abs(Offset.Size2D() / PreferredLegLength - 1.0))
			: 1.0;
		TotalWeight += (1.0 + 4.0 * FMath::Square(FMath::Max(0.0, Alignment))) * LengthWeight;
		CumulativeWeights.Add(TotalWeight);
	}
	const double Roll = FMath::FRandRange(0.0, TotalWeight);
	for (int32 Index = 0; Index < Choices.Num(); ++Index)
	{
		if (Roll < CumulativeWeights[Index]) return Choices[Index];
	}
	return Choices.Last();
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
