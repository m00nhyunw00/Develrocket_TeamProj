#include "Components/DaySystemComponent.h"

#include "AbilitySystem/Attributes/TBAttributeSetBase.h"
#include "AbilitySystemComponent.h"
#include "Character/Player/TBPlayer.h"
#include "Components/DirectionalLightComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/DirectionalLight.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Components/SceneComponent.h"
#include "EngineUtils.h"
#include "Framework/TBLevelManager.h"
#include "GameplayEffect.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TBGameplayTags.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/SoftObjectPath.h"

ADayCycleController::ADayCycleController()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);
	PrimaryActorTick.bCanEverTick = true;
}

void ADayCycleController::InitializeCycle(ADirectionalLight* Light, float CycleSeconds)
{
	if (!HasAuthority() || !IsValid(Light) || CycleState.bInitialized) return;
	if (!Light->GetRootComponent() || Light->GetRootComponent()->Mobility != EComponentMobility::Movable)
	{
		UE_LOG(LogTemp, Warning, TEXT("DaySystemComponent: Directional Light의 Mobility를 Movable로 설정하세요."));
		return;
	}
	LocalLight = Light;
	CycleState.LightName = Light->GetFName();
	CycleState.InitialRotation = Light->GetActorRotation();
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	CycleState.StartServerTime = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	CycleState.CycleSeconds = FMath::Max(1.0f, CycleSeconds);
	CycleState.bInitialized = true;
	ForceNetUpdate();
}

void ADayCycleController::OnRep_CycleState()
{
	LocalLight = nullptr;
	UpdateSunRotation();
}

void ADayCycleController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateSunRotation();
}

void ADayCycleController::UpdateSunRotation()
{
	if (!CycleState.bInitialized || !GetWorld()) return;
	if (!IsValid(LocalLight))
	{
		for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
		{
			if (It->GetFName() == CycleState.LightName) { LocalLight = *It; break; }
		}
	}
	if (!IsValid(LocalLight)) return;
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	if (!GameState && GetNetMode() == NM_Client) return;
	const float ServerTime = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	const float Elapsed = FMath::Max(0.0f, ServerTime - CycleState.StartServerTime);
	FRotator Rotation = CycleState.InitialRotation;
	Rotation.Pitch += FMath::Fmod(Elapsed / CycleState.CycleSeconds, 1.0f) * 360.0f;
	LocalLight->SetActorRotation(Rotation);
}

void ADayCycleController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADayCycleController, CycleState);
}

UDaySystemComponent::UDaySystemComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	static const ConstructorHelpers::FClassFinder<UGameplayEffect> DefaultSunlightDamageEffect(
		TEXT("/Game/_Game/Blueprints/AbilitySystem/GE_SunlightDamage"));
	SunlightDamageEffect = DefaultSunlightDamageEffect.Class;
}

void UDaySystemComponent::BeginPlay()
{
	Super::BeginPlay();
	const ATBLevelManager* Manager = Cast<ATBLevelManager>(GetOwner());
	if (!Manager || !Manager->HasAuthority()) return;

	if (!ExplicitSunLight.IsValid() && IsValid(DirectionalLight))
	{
		ExplicitSunLight = DirectionalLight->GetComponent();
	}

	// 명시된 태양이 없을 때 Directional Light가 하나인 레벨만 자동 연결
	if (!IsValid(DirectionalLight))
	{
		TArray<AActor*> Lights;
		UGameplayStatics::GetAllActorsOfClass(this, ADirectionalLight::StaticClass(), Lights);
		if (Lights.Num() == 1) DirectionalLight = Cast<ADirectionalLight>(Lights[0]);
	}

	EnsureDayCycleController();
	TraceSunlight();
	GetWorld()->GetTimerManager().SetTimer(TraceTimer, this, &ThisClass::TraceSunlight, FMath::Max(0.05f, TraceInterval), true);
}

void UDaySystemComponent::SetDirectionalLight(ADirectionalLight* InLight)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	UDaySystemComponent* LevelDaySystem = GetLevelDaySystem();
	if (!LevelDaySystem) return;
	if (LevelDaySystem != this)
	{
		LevelDaySystem->SetDirectionalLight(InLight);
		return;
	}
	DirectionalLight = InLight;
	ExplicitSunLight = IsValid(InLight) ? InLight->GetComponent() : nullptr;
	NextSunlightDamageTimes.Reset();
	EnsureDayCycleController();
	TraceSunlight();
}

void UDaySystemComponent::SetSunLightComponent(UDirectionalLightComponent* InLight)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	UDaySystemComponent* LevelDaySystem = GetLevelDaySystem();
	if (!LevelDaySystem) return;
	if (LevelDaySystem != this)
	{
		LevelDaySystem->SetSunLightComponent(InLight);
		return;
	}
	ExplicitSunLight = InLight;
	NextSunlightDamageTimes.Reset();
	TraceSunlight();
}

UDaySystemComponent* UDaySystemComponent::GetLevelDaySystem() const
{
	const ATBLevelManager* Manager = Cast<ATBLevelManager>(GetOwner());
	if (!Manager)
	{
		Manager = Cast<ATBLevelManager>(UGameplayStatics::GetActorOfClass(this, ATBLevelManager::StaticClass()));
	}
	return Manager ? Manager->DaySystem.Get() : nullptr;
}

void UDaySystemComponent::EnsureDayCycleController()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(DirectionalLight) || !GetWorld()) return;
	for (TActorIterator<ADayCycleController> It(GetWorld()); It; ++It)
	{
		It->InitializeCycle(DirectionalLight, FullCycleSeconds);
		return;
	}
	if (ADayCycleController* Controller = GetWorld()->SpawnActor<ADayCycleController>())
	{
		Controller->InitializeCycle(DirectionalLight, FullCycleSeconds);
	}
}

UDirectionalLightComponent* UDaySystemComponent::ResolveSunLight(double Now)
{
	if (UDirectionalLightComponent* Light = ExplicitSunLight.Get()) return Light;
	if (UDirectionalLightComponent* Light = CitySunLight.Get()) return Light;

	if (Now >= NextSunSearchTime)
	{
		// The city graph can spawn the sky Blueprint after the level manager begins play.
		NextSunSearchTime = Now + 1.0;
		static const FSoftClassPath DayNightCycleClass(
			TEXT("/Game/_Assets/_Original/DayNightCycle/Components/BP_DayNightCycle.BP_DayNightCycle_C"));
		static const FName SunComponentName(TEXT("DirectionalLight"));
		if (UClass* SkyClass = DayNightCycleClass.ResolveClass())
		{
			for (TActorIterator<AActor> It(GetWorld(), SkyClass); It; ++It)
			{
				TInlineComponentArray<UDirectionalLightComponent*> Lights(*It);
				for (UDirectionalLightComponent* Light : Lights)
				{
					if (Light->GetFName() == SunComponentName)
					{
						CitySunLight = Light;
						return Light;
					}
					if (Light->IsUsedAsAtmosphereSunLight() && Light->GetAtmosphereSunLightIndex() == 0)
					{
						CitySunLight = Light;
					}
				}
				if (CitySunLight.IsValid()) return CitySunLight.Get();
			}
		}
	}

	return IsValid(DirectionalLight) ? DirectionalLight->GetComponent() : nullptr;
}

void UDaySystemComponent::TraceSunlight()
{
	const ATBLevelManager* Manager = Cast<ATBLevelManager>(GetOwner());
	UWorld* World = GetWorld();
	if (!Manager || !Manager->HasAuthority() || !World) return;

	const double Now = World->GetTimeSeconds();
	UDirectionalLightComponent* Sun = ResolveSunLight(Now);
	bHasValidSunTrace = IsValid(Sun);
	bSunTraceClear = false;
	const FVector TowardSun = bHasValidSunTrace ? -Sun->GetForwardVector() : FVector::ZeroVector;
	const bool bSunIsUp = TowardSun.Z > 0.0 && Sun->Intensity > 0.0f && Sun->IsVisible();
	const bool bMatchReady = Manager->IsLocalCityReady() && Manager->IsMatchStarted();
	const FVector Ray = TowardSun * FMath::Max(1.0f, TraceDistance);

	for (auto It = NextSunlightDamageTimes.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) It.RemoveCurrent();
	}

	for (TActorIterator<ATBPlayer> It(World); It; ++It)
	{
		ATBPlayer& Player = **It;
		const bool bValidPlayerTrace = bHasValidSunTrace && bMatchReady && !Player.IsDead();
		const bool bExposed = bValidPlayerTrace && bSunIsUp && TracePlayer(Player, *Sun, Ray);
		bSunTraceClear |= bExposed;

		// Keep existing character Blueprints' sunlight queries working without a second timer.
		if (UDaySystemComponent* PlayerDaySystem = Player.FindComponentByClass<UDaySystemComponent>())
		{
			PlayerDaySystem->bHasValidSunTrace = bValidPlayerTrace;
			PlayerDaySystem->bSunTraceClear = bExposed;
		}

		UpdateSunlightDamage(Player, Now, bExposed);
	}
}

bool UDaySystemComponent::TracePlayer(ATBPlayer& Player, const UDirectionalLightComponent& Sun, const FVector& Ray)
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SunlightTrace), false, &Player);
	Params.AddIgnoredActor(Sun.GetOwner());

	const FVector Head = Player.GetActorLocation() + TraceStartOffset;
	const FVector Chest = Head - FVector(0.0, 0.0, TraceStartOffset.Z * 0.5);
	const auto IsExposedAt = [&](const FVector& Start)
	{
		const FVector End = Start + Ray;
		const bool bClear = !GetWorld()->LineTraceTestByChannel(Start, End, TraceChannel, Params);
		if (bDrawDebugTrace) MulticastDrawTrace(Start, End, bClear);
		return bClear;
	};
	return IsExposedAt(Head) || IsExposedAt(Chest);
}

void UDaySystemComponent::UpdateSunlightDamage(ATBPlayer& Player, double Now, bool bExposed)
{
	const TWeakObjectPtr<ATBPlayer> PlayerKey(&Player);
	if (!bEnableSunlightDamage || !SunlightDamageEffect || !bExposed
		|| !FMath::IsFinite(SunlightDamagePerSecond) || SunlightDamagePerSecond <= 0.0f)
	{
		NextSunlightDamageTimes.Remove(PlayerKey);
		return;
	}

	UAbilitySystemComponent* ASC = Player.GetAbilitySystemComponent();
	const FGameplayAttribute BloodAttribute = UTBAttributeSetBase::GetBloodAttribute();
	if (!IsValid(ASC) || ASC->GetAvatarActor() != &Player || !ASC->HasAttributeSetForAttribute(BloodAttribute)
		|| ASC->GetNumericAttribute(BloodAttribute) <= 0.0f
		|| ASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead)
		|| ASC->HasMatchingGameplayTag(TBGameplayTags::State_Invulnerable))
	{
		NextSunlightDamageTimes.Remove(PlayerKey);
		return;
	}

	double& NextDamageTime = NextSunlightDamageTimes.FindOrAdd(PlayerKey,
		Now + FMath::Max(0.0f, SunlightExposureGracePeriod));
	if (Now < NextDamageTime) return;
	NextDamageTime = Now + 1.0;
	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(
		SunlightDamageEffect, 1.0f, ASC->MakeEffectContext());
	if (Spec.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(TBGameplayTags::Data_Damage, -SunlightDamagePerSecond);
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}

void UDaySystemComponent::MulticastDrawTrace_Implementation(FVector Start, FVector End, bool bClear)
{
#if ENABLE_DRAW_DEBUG
	if (GetWorld() && GetWorld()->GetNetMode() != NM_DedicatedServer)
	{
		DrawDebugLine(GetWorld(), Start, End, bClear ? FColor::Green : FColor::Red, false, FMath::Max(0.05f, TraceInterval) * 1.2f, 0, 2.0f);
	}
#endif
}

void UDaySystemComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UDaySystemComponent, bSunTraceClear);
	DOREPLIFETIME(UDaySystemComponent, bHasValidSunTrace);
}

void UDaySystemComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(TraceTimer);
	Super::EndPlay(EndPlayReason);
}
