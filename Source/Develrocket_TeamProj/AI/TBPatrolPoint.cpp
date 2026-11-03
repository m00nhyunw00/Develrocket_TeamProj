#include "AI/TBPatrolPoint.h"
#include "Components/BillboardComponent.h"

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
}

void ATBPatrolPoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}