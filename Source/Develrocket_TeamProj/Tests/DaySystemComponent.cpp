#include "Tests/DaySystemComponent.h"

#include "DrawDebugHelpers.h"
#include "Engine/DirectionalLight.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Components/SceneComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

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

	// 명시된 태양이 없을 때 Directional Light가 하나인 레벨만 자동 연결
	if (!IsValid(DirectionalLight))
	{
		TArray<AActor*> Lights;
		UGameplayStatics::GetAllActorsOfClass(this, ADirectionalLight::StaticClass(), Lights);
		if (Lights.Num() == 1) DirectionalLight = Cast<ADirectionalLight>(Lights[0]);
		else UE_LOG(LogTemp, Warning, TEXT("DaySystemComponent: Directional Light를 명시적으로 지정하세요. 검색된 개수: %d"), Lights.Num());
	}

	EnsureDayCycleController();
	TraceSunlight();
	GetWorld()->GetTimerManager().SetTimer(TraceTimer, this, &ThisClass::TraceSunlight, FMath::Max(0.05f, TraceInterval), true);
}

void UDaySystemComponent::SetDirectionalLight(ADirectionalLight* InLight)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	DirectionalLight = InLight;
	EnsureDayCycleController();
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

void UDaySystemComponent::TraceSunlight()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(DirectionalLight) || !GetWorld())
	{
		bHasValidSunTrace = false;
		bSunTraceClear = false;
		return;
	}

	const FVector Start = GetOwner()->GetActorLocation() + TraceStartOffset;
	const FVector End = Start - DirectionalLight->GetActorForwardVector() * FMath::Max(1.0f, TraceDistance);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SunlightTrace), false, GetOwner());
	Params.AddIgnoredActor(DirectionalLight);
	FHitResult Hit;
	const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, TraceChannel, Params);
	bHasValidSunTrace = true;
	bSunTraceClear = !bBlocked;
	if (bDrawDebugTrace) MulticastDrawTrace(Start, End, bSunTraceClear);
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
