#include "Tests/DaySystemComponent.h"

#include "AbilitySystem/Attributes/TBAttributeSetBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
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
}

void UDaySystemComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	LevelManager = Cast<ATBLevelManager>(UGameplayStatics::GetActorOfClass(this, ATBLevelManager::StaticClass()));
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
	DirectionalLight = InLight;
	ExplicitSunLight = IsValid(InLight) ? InLight->GetComponent() : nullptr;
	NextSunlightDamageTime.Reset();
	EnsureDayCycleController();
	TraceSunlight();
}

void UDaySystemComponent::SetSunLightComponent(UDirectionalLightComponent* InLight)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	ExplicitSunLight = InLight;
	NextSunlightDamageTime.Reset();
	TraceSunlight();
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
		// The city graph spawns the sky BP after the character may have begun play.
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
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !Owner->HasAuthority() || !World) return;

	const double Now = World->GetTimeSeconds();
	UDirectionalLightComponent* Sun = ResolveSunLight(Now);
	bHasValidSunTrace = IsValid(Sun);
	bSunTraceClear = false;
	if (bHasValidSunTrace)
	{
		const FVector TowardSun = -Sun->GetForwardVector();
		if (TowardSun.Z > 0.0 && Sun->Intensity > 0.0f && Sun->IsVisible())
		{
			const FVector Ray = TowardSun * FMath::Max(1.0f, TraceDistance);
			FCollisionQueryParams Params(SCENE_QUERY_STAT(SunlightTrace), false, Owner);
			Params.AddIgnoredActor(Sun->GetOwner());

			const FVector Head = Owner->GetActorLocation() + TraceStartOffset;
			const FVector Chest = Head - FVector(0.0, 0.0, TraceStartOffset.Z * 0.5);
			const auto IsExposedAt = [&](const FVector& Start)
			{
				const FVector End = Start + Ray;
				const bool bClear = !World->LineTraceTestByChannel(Start, End, TraceChannel, Params);
				if (bDrawDebugTrace) MulticastDrawTrace(Start, End, bClear);
				return bClear;
			};
			bSunTraceClear = IsExposedAt(Head) || IsExposedAt(Chest);
		}
	}

	UpdateSunlightDamage(Now);
}

void UDaySystemComponent::UpdateSunlightDamage(double Now)
{
	const ATBLevelManager* Manager = LevelManager.Get();
	if (!bEnableSunlightDamage || !SunlightDamageEffect || !bSunTraceClear
		|| !FMath::IsFinite(SunlightDamagePerSecond) || SunlightDamagePerSecond <= 0.0f
		|| (Manager && (!Manager->IsMatchStarted() || !Manager->IsLocalCityReady())))
	{
		NextSunlightDamageTime.Reset();
		return;
	}

	AActor* Owner = GetOwner();
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner);
	const FGameplayAttribute BloodAttribute = UTBAttributeSetBase::GetBloodAttribute();
	if (!IsValid(ASC) || ASC->GetAvatarActor() != Owner || !ASC->HasAttributeSetForAttribute(BloodAttribute)
		|| ASC->GetNumericAttribute(BloodAttribute) <= 0.0f
		|| ASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead)
		|| ASC->HasMatchingGameplayTag(TBGameplayTags::State_Invulnerable))
	{
		NextSunlightDamageTime.Reset();
		return;
	}

	if (!NextSunlightDamageTime.IsSet())
	{
		NextSunlightDamageTime = Now + FMath::Max(0.0f, SunlightExposureGracePeriod);
	}
	if (Now < NextSunlightDamageTime.GetValue()) return;

	NextSunlightDamageTime = Now + 1.0;
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
