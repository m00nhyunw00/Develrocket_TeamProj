#if WITH_DEV_AUTOMATION_TESTS

#include "AI/TBCombatTokenSubsystem.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Character/Monster/TBMonster.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBCombatTokenTest, "Develrocket.AI.CombatTokens",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTBCombatTokenTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	UTBCombatTokenSubsystem* Tokens = World->GetSubsystem<UTBCombatTokenSubsystem>();
	if (!TestNotNull(TEXT("Combat subsystem exists in a game world"), Tokens)) return false;

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ATBMonster* First = World->SpawnActor<ATBMonster>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
	ATBMonster* Second = World->SpawnActor<ATBMonster>(FVector(200.0, 0.0, 0.0), FRotator::ZeroRotator, SpawnParameters);
	ATBMonster* Waiting = World->SpawnActor<ATBMonster>(FVector(400.0, 0.0, 0.0), FRotator::ZeroRotator, SpawnParameters);
	ATBMonster* Other = World->SpawnActor<ATBMonster>(FVector(600.0, 0.0, 0.0), FRotator::ZeroRotator, SpawnParameters);
	AActor* Target = World->SpawnActor<AActor>();
	AActor* OtherTarget = World->SpawnActor<AActor>();
	if (!First || !Second || !Waiting || !Other || !Target || !OtherTarget)
	{
		AddError(TEXT("Could not spawn token test actors"));
		return false;
	}

	const FGameplayAbilitySpecHandle Ability = FGameplayAbilitySpec(UGameplayAbility::StaticClass()).Handle;
	const auto Acquire = [Tokens, Ability](ATBMonster& Monster, AActor& Victim)
	{
		return Tokens->TryAcquire(Monster, Victim, *Monster.GetAbilitySystemComponent(), Ability);
	};
	const FTBCombatToken FirstToken = Acquire(*First, *Target);
	const FTBCombatToken SecondToken = Acquire(*Second, *Target);
	TestTrue(TEXT("Target is full after two reservations"), Tokens->IsHeld(FirstToken) && Tokens->IsHeld(SecondToken)
		&& Tokens->IsAttackFull(*Target));
	TestFalse(TEXT("A third attacker cannot reserve a token"), Acquire(*Waiting, *Target).Id != 0);
	const FTBCombatToken OtherToken = Acquire(*Other, *OtherTarget);
	TestTrue(TEXT("Another target has an independent budget"), Tokens->IsHeld(OtherToken));
	TestFalse(TEXT("One monster cannot attack two targets at once"), Acquire(*First, *OtherTarget).Id != 0);

	Tokens->SetWaitingSlot(*Target, *Waiting, 0);
	TestFalse(TEXT("Waiting slots are exclusive within a target"), Tokens->IsWaitingSlotAvailable(*Target, *First, 0));
	TestTrue(TEXT("Another target can use the same slot index"), Tokens->IsWaitingSlotAvailable(*OtherTarget, *First, 0));

	Tokens->ReleaseIfInactive(FirstToken);
	TestFalse(TEXT("A recently released attacker cannot overtake a ready waiting attacker"), Acquire(*First, *Target).Id != 0);
	const FTBCombatToken WaitingToken = Acquire(*Waiting, *Target);
	TestTrue(TEXT("The waiting attacker gets the next token"), Tokens->IsHeld(WaitingToken));
	TestTrue(TEXT("The attacker frees its waiting position on acquisition"), Tokens->IsWaitingSlotAvailable(*Target, *First, 0));

	Second->GetAbilitySystemComponent()->OnAbilityEnded.Broadcast(FAbilityEndedData(nullptr, Ability, false, true));
	TestFalse(TEXT("GAS cancellation returns the token"), Tokens->IsHeld(SecondToken));
	Tokens->ClearTarget(*Waiting, Target);
	TestFalse(TEXT("Clearing a target returns a reserved token"), Tokens->IsHeld(WaitingToken));
	TestTrue(TEXT("Clearing a target preserves unrelated tokens"), Tokens->IsHeld(OtherToken));
	return true;
}

#endif
