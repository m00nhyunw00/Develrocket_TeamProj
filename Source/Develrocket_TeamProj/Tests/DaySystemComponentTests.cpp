#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/DaySystemComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDayCycleSingleControllerTest, "Project.DaySystem.SingleControllerAndRotation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDayCycleSingleControllerTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Values = UWorld::InitializationValues()
		.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false)
		.ShouldSimulatePhysics(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	if (!TestNotNull(TEXT("Test world"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);

	ADirectionalLight* Light = World->SpawnActor<ADirectionalLight>();
	if (!TestNotNull(TEXT("Directional light"), Light)) { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); return false; }
	Light->GetRootComponent()->SetMobility(EComponentMobility::Movable);
	const FRotator InitialRotation = Light->GetActorRotation();

	for (int32 Index = 0; Index < 2; ++Index)
	{
		AActor* Owner = World->SpawnActor<AActor>();
		UDaySystemComponent* Component = NewObject<UDaySystemComponent>(Owner);
		Owner->AddInstanceComponent(Component);
		Component->RegisterComponent();
		Owner->DispatchBeginPlay();
	}

	ADayCycleController* Controller = nullptr;
	int32 ControllerCount = 0;
	for (TActorIterator<ADayCycleController> It(World); It; ++It) { Controller = *It; ++ControllerCount; }
	TestEqual(TEXT("Multiple player components share one controller"), ControllerCount, 1);
	if (TestNotNull(TEXT("Day cycle controller"), Controller))
	{
		World->Tick(LEVELTICK_All, 1.0f);
		Controller->Tick(0.0f);
		TestFalse(TEXT("Directional light rotates as world time advances"), Light->GetActorRotation().Equals(InitialRotation, 0.1f));
	}
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif
