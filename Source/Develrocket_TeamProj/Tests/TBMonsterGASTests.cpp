#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AIController.h"
#include "AbilitySystem/Abilities/TBGameplayAbility_BloodAction.h"
#include "AbilitySystem/Attributes/TBMonsterAttributeSet.h"
#include "AbilitySystem/TBAbilitySystemComponent.h"
#include "Character/Monster/TBMonster.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "TBGameplayTags.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"

namespace
{
	struct FScopedMonsterTestWorld
	{
		UWorld* World = nullptr;

		FScopedMonsterTestWorld()
		{
			const UWorld::InitializationValues Values = UWorld::InitializationValues()
				.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false)
				.ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
		}

		~FScopedMonsterTestWorld()
		{
			if (World)
			{
				World->DestroyWorld(false);
			}
		}
	};

	void ApplyAttributeOverride(UAbilitySystemComponent* ASC, const FGameplayAttribute& Attribute, float Value)
	{
		UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage());
		Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::Override;
		Modifier.ModifierMagnitude = FScalableFloat(Value);
		Effect->Modifiers.Add(Modifier);
		ASC->ApplyGameplayEffectSpecToSelf(FGameplayEffectSpec(Effect, ASC->MakeEffectContext(), 1.0f));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBMonsterDeathTagTest, "Project.GAS.Monster.DeathEffectTag", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTBMonsterDeathTagTest::RunTest(const FString& Parameters)
{
	FScopedMonsterTestWorld TestWorld;
	if (!TestNotNull(TEXT("Isolated game world"), TestWorld.World)) return false;

	UClass* DeathEffect = LoadClass<UGameplayEffect>(nullptr, TEXT("/Game/_Game/Blueprints/AbilitySystem/GE_Dead.GE_Dead_C"));
	FClassProperty* DeathEffectProperty = FindFProperty<FClassProperty>(ATBMonster::StaticClass(), TEXT("DeathEffectClass"));
	if (!TestNotNull(TEXT("Death GE class"), DeathEffect) || !TestNotNull(TEXT("Death GE setting"), DeathEffectProperty)) return false;

	ATBMonster* Monster = TestWorld.World->SpawnActorDeferred<ATBMonster>(ATBMonster::StaticClass(), FTransform::Identity, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!TestNotNull(TEXT("Monster"), Monster)) return false;
	DeathEffectProperty->SetObjectPropertyValue_InContainer(Monster, DeathEffect);
	Monster->FinishSpawning(FTransform::Identity);
	if (!Monster->HasActorBegunPlay()) Monster->DispatchBeginPlay();
	UTBAbilitySystemComponent* ASC = Monster->GetTBAbilitySystemComponent();
	if (!TestNotNull(TEXT("Monster ASC"), ASC)) return false;

	ApplyAttributeOverride(ASC, UTBMonsterAttributeSet::GetBloodAttribute(), 0.0f);
	TestTrue(TEXT("Death GE grants State.Dead"), ASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead));
	TestTrue(TEXT("IsDead reads State.Dead"), Monster->IsDead());
	TestTrue(TEXT("Death disables movement"), Monster->GetCharacterMovement()->MovementMode == MOVE_None);
	TestEqual(TEXT("Death disables collision"), Monster->GetCapsuleComponent()->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	bool bFoundDeathEffect = false;
	for (const FActiveGameplayEffectHandle& Handle : ASC->GetActiveEffects(FGameplayEffectQuery()))
	{
		const FActiveGameplayEffect* ActiveEffect = ASC->GetActiveGameplayEffect(Handle);
		bFoundDeathEffect |= ActiveEffect && ActiveEffect->Spec.Def && ActiveEffect->Spec.Def->GetClass() == DeathEffect;
	}
	TestTrue(TEXT("GE_Dead remains active"), bFoundDeathEffect);

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ATBMonster* UnconfiguredMonster = TestWorld.World->SpawnActor<ATBMonster>(ATBMonster::StaticClass(), FTransform::Identity, SpawnParameters);
	if (!TestNotNull(TEXT("Monster without death GE"), UnconfiguredMonster)) return false;
	if (!UnconfiguredMonster->HasActorBegunPlay()) UnconfiguredMonster->DispatchBeginPlay();
	AddExpectedError(TEXT("DeathEffectClass"), EAutomationExpectedErrorFlags::Contains, 1);
	ApplyAttributeOverride(UnconfiguredMonster->GetTBAbilitySystemComponent(), UTBMonsterAttributeSet::GetBloodAttribute(), 0.0f);
	TestFalse(TEXT("Missing death GE does not add a fallback tag"), UnconfiguredMonster->IsDead());
	TestEqual(TEXT("Missing death GE leaves collision enabled"), UnconfiguredMonster->GetCapsuleComponent()->GetCollisionEnabled(), ECollisionEnabled::QueryAndPhysics);
	DeathEffectProperty->SetObjectPropertyValue_InContainer(UnconfiguredMonster, UGameplayEffect::StaticClass());
	AddExpectedError(TEXT("사망 GE 적용에 실패했거나 State.Dead"), EAutomationExpectedErrorFlags::Contains, 1);
	ApplyAttributeOverride(UnconfiguredMonster->GetTBAbilitySystemComponent(), UTBMonsterAttributeSet::GetBloodAttribute(), 0.0f);
	TestFalse(TEXT("Death GE without State.Dead does not add a fallback tag"), UnconfiguredMonster->IsDead());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBMonsterGASLifecycleTest, "Project.GAS.Monster.InitializationDamageAndDeath", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTBMonsterGASLifecycleTest::RunTest(const FString& Parameters)
{
	FScopedMonsterTestWorld TestWorld;
	if (!TestNotNull(TEXT("Isolated game world"), TestWorld.World)) return false;

	ATBMonster* Monster = TestWorld.World->SpawnActorDeferred<ATBMonster>(ATBMonster::StaticClass(), FTransform::Identity, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!TestNotNull(TEXT("GAS monster"), Monster)) return false;

	// 기존 근접 슬롯과 새 목록에 같은 Ability를 넣어 중복 부여 검사
	FClassProperty* MeleeProperty = FindFProperty<FClassProperty>(ATBMonster::StaticClass(), TEXT("MeleeAbilityClass"));
	FArrayProperty* StartupProperty = FindFProperty<FArrayProperty>(ATBMonster::StaticClass(), TEXT("StartupAbilities"));
	if (!TestNotNull(TEXT("Legacy melee configuration"), MeleeProperty) || !TestNotNull(TEXT("Startup ability configuration"), StartupProperty)) return false;
	MeleeProperty->SetObjectPropertyValue_InContainer(Monster, UTBGameplayAbility_PrimaryAttack::StaticClass());
	TArray<TSubclassOf<UGameplayAbility>>& StartupAbilities = *StartupProperty->ContainerPtrToValuePtr<TArray<TSubclassOf<UGameplayAbility>>>(Monster);
	StartupAbilities.Add(UTBGameplayAbility_PrimaryAttack::StaticClass());
	StartupAbilities.Add(UTBGameplayAbility_Reload::StaticClass());
	StartupAbilities.Add(UTBGameplayAbility_Reload::StaticClass());
	Monster->FinishSpawning(FTransform::Identity);
	if (!Monster->HasActorBegunPlay()) Monster->DispatchBeginPlay();

	UTBAbilitySystemComponent* ASC = Monster->GetTBAbilitySystemComponent();
	if (!TestNotNull(TEXT("Monster uses the project ASC"), ASC)) return false;
	TestTrue(TEXT("ASC owner and avatar point to the monster"), ASC->GetOwnerActor() == Monster && ASC->GetAvatarActor() == Monster);
	TestEqual(TEXT("Initial health GE"), Monster->GetHealth(), 100.0f);
	TestEqual(TEXT("Initial maximum health GE"), Monster->GetMaxHealth(), 100.0f);
	TestEqual(TEXT("Distinct startup abilities granted once"), ASC->GetActivatableAbilities().Num(), 2);

	TestTrue(TEXT("Server damage GE applied"), Monster->ApplyGASDamage(40.0f, nullptr));
	TestEqual(TEXT("Damage subtracts health"), Monster->GetHealth(), 60.0f);
	TestEqual(TEXT("Damage meta attribute resets"), ASC->GetNumericAttribute(UTBMonsterAttributeSet::GetIncomingDamageAttribute()), 0.0f);
	TestFalse(TEXT("Negative damage rejected"), Monster->ApplyGASDamage(-5.0f, nullptr));
	TestFalse(TEXT("Zero damage rejected"), Monster->ApplyGASDamage(0.0f, nullptr));
	TestEqual(TEXT("Rejected damage cannot heal"), Monster->GetHealth(), 60.0f);

	AAIController* Controller = TestWorld.World->SpawnActor<AAIController>();
	if (!TestNotNull(TEXT("Controller for repossession"), Controller)) return false;
	Controller->Possess(Monster);
	TestEqual(TEXT("Repossession does not reset health"), Monster->GetHealth(), 60.0f);
	TestEqual(TEXT("Repossession does not duplicate abilities"), ASC->GetActivatableAbilities().Num(), 2);

	ApplyAttributeOverride(ASC, UTBMonsterAttributeSet::GetBloodAttribute(), 500.0f);
	TestEqual(TEXT("Healing is capped by maximum health"), Monster->GetHealth(), 100.0f);
	ApplyAttributeOverride(ASC, UTBMonsterAttributeSet::GetMaxBloodAttribute(), 25.0f);
	TestEqual(TEXT("Maximum health changes clamp current health"), Monster->GetHealth(), 25.0f);
	TestTrue(TEXT("Lethal damage GE applied"), Monster->ApplyGASDamage(1000.0f, nullptr));
	TestTrue(TEXT("Monster enters terminal death state"), Monster->IsDead());
	TestEqual(TEXT("Lethal damage clamps health to zero"), Monster->GetHealth(), 0.0f);
	TestTrue(TEXT("Death gameplay effect grants state tag"), ASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead));
	TestTrue(TEXT("Death blocks combat ability tags"), ASC->AreAbilityTagsBlocked(FGameplayTagContainer(TBGameplayTags::Ability_Combat_PrimaryAttack)));
	TestTrue(TEXT("Death disables movement"), Monster->GetCharacterMovement()->MovementMode == MOVE_None);
	TestEqual(TEXT("Death disables capsule collision"), Monster->GetCapsuleComponent()->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	TestFalse(TEXT("Damage after death rejected"), Monster->ApplyGASDamage(10.0f, nullptr));
	ApplyAttributeOverride(ASC, UTBMonsterAttributeSet::GetBloodAttribute(), 100.0f);
	TestEqual(TEXT("Ordinary healing does not resurrect a dead monster"), Monster->GetHealth(), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBMonsterBlueprintCompatibilityTest, "Project.GAS.Monster.BlueprintCompatibility", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTBMonsterBlueprintCompatibilityTest::RunTest(const FString& Parameters)
{
	FScopedMonsterTestWorld TestWorld;
	if (!TestNotNull(TEXT("Blueprint compatibility world"), TestWorld.World)) return false;

	FSoftClassPath LegacyClassPath(TEXT("/Script/Develrocket_TeamProj.TBGASMonster"));
	TestTrue(TEXT("Serialized class reference has a Core Redirect"), LegacyClassPath.FixupCoreRedirects());
	UClass* RedirectedClass = LegacyClassPath.TryLoadClass<ATBMonster>();
	TestTrue(TEXT("Removed native class redirects to TBMonster"), RedirectedClass == ATBMonster::StaticClass());

	const TCHAR* BlueprintPaths[] = {
		TEXT("/Game/_Game/Blueprints/Characters/BP_TBMonster.BP_TBMonster_C"),
		TEXT("/Game/_Game/Blueprints/Characters/BP_TBMonster_Boris.BP_TBMonster_Boris_C")
	};
	for (const TCHAR* Path : BlueprintPaths)
	{
		UClass* MonsterClass = LoadClass<ATBMonster>(nullptr, Path);
		if (!TestNotNull(FString::Printf(TEXT("Load %s"), Path), MonsterClass)) continue;
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ATBMonster* Monster = TestWorld.World->SpawnActor<ATBMonster>(MonsterClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
		if (!TestNotNull(FString::Printf(TEXT("Spawn %s"), Path), Monster)) continue;
		if (!Monster->HasActorBegunPlay()) Monster->DispatchBeginPlay();
		TestNotNull(FString::Printf(TEXT("Custom ASC in %s"), Path), Monster->GetTBAbilitySystemComponent());
		TestEqual(FString::Printf(TEXT("Initial health in %s"), Path), Monster->GetHealth(), 100.0f);
		TestTrue(FString::Printf(TEXT("Damage in %s"), Path), Monster->ApplyGASDamage(40.0f, nullptr));
		TestEqual(FString::Printf(TEXT("Remaining health in %s"), Path), Monster->GetHealth(), 60.0f);
		TestTrue(FString::Printf(TEXT("Lethal damage in %s"), Path), Monster->ApplyGASDamage(1000.0f, nullptr));
		TestTrue(FString::Printf(TEXT("Death in %s"), Path), Monster->IsDead());
	}
	return true;
}

#endif
