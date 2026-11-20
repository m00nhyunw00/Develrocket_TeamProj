#if WITH_DEV_AUTOMATION_TESTS

#include "Components/TBPoolStateComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBPoolStateRestoresCollisionAndTick,
	"Project.SpawnPool.RestoresCollisionAndMovementTick",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTBPoolStateRestoresCollisionAndTick::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Values = UWorld::InitializationValues()
		.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false)
		.ShouldSimulatePhysics(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	if (!TestNotNull(TEXT("Test world"), World)) return false;

	ACharacter* Character = World->SpawnActor<ACharacter>();
	if (TestNotNull(TEXT("Pooled character"), Character))
	{
		UTBPoolStateComponent* PoolState = NewObject<UTBPoolStateComponent>(Character);
		Character->AddInstanceComponent(PoolState);
		PoolState->RegisterComponent();
		UCharacterMovementComponent* Movement = Character->GetCharacterMovement();

		Character->SetActorEnableCollision(true);
		Movement->SetComponentTickEnabled(true);
		PoolState->SetPoolActive(false);
		TestTrue(TEXT("Inactive character is hidden"), Character->IsHidden());
		TestFalse(TEXT("Inactive character has no collision"), Character->GetActorEnableCollision());
		TestFalse(TEXT("Inactive movement component does not tick"), Movement->IsComponentTickEnabled());

		PoolState->SetPoolActive(true);
		TestFalse(TEXT("Reactivated character is visible"), Character->IsHidden());
		TestTrue(TEXT("Reactivation restores collision"), Character->GetActorEnableCollision());
		TestTrue(TEXT("Reactivation restores movement tick"), Movement->IsComponentTickEnabled());

		Character->SetActorEnableCollision(false);
		Movement->SetComponentTickEnabled(false);
		PoolState->SetPoolActive(false);
		PoolState->SetPoolActive(true);
		TestFalse(TEXT("Reactivation preserves previously disabled collision"), Character->GetActorEnableCollision());
		TestFalse(TEXT("Reactivation preserves previously disabled movement tick"), Movement->IsComponentTickEnabled());
	}
	World->DestroyWorld(false);
	return true;
}

#endif
