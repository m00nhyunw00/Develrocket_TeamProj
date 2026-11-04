#include "AI/TBPatrolPoint.h"
#include "Components/BillboardComponent.h"
#include "AI/TBPatrolPointSubsystem.h"

ATBPatrolPoint::ATBPatrolPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	
	SetActorEnableCollision(false);
	
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