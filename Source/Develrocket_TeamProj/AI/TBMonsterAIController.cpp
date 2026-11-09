#include "AI/TBMonsterAIController.h"

#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/Player/TBPlayer.h"
#include "Character/Monster/TBMonster.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "TimerManager.h"

ATBMonsterAIController::ATBMonsterAIController()
{
	UAIPerceptionComponent* PerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComp"));
	SetPerceptionComponent(* PerceptionComp);
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	PerceptionComp->ConfigureSense(*SightConfig);
	PerceptionComp->SetDominantSense(UAISense_Sight::StaticClass());
}

void ATBMonsterAIController::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		return;
	}
	
	SightConfig->SightRadius = SightRadius;
	SightConfig->LoseSightRadius = LoseSightRadius;
	SightConfig->PeripheralVisionAngleDegrees = PeripheralVisionAngle;
	
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	
	GetPerceptionComponent()->ConfigureSense(*SightConfig);
	GetPerceptionComponent()->SetDominantSense(SightConfig->GetSenseImplementation());
	GetPerceptionComponent()->OnTargetPerceptionUpdated.AddDynamic(this, &ThisClass::HandleTargetPerceptionUpdated);
	GetPerceptionComponent()->RequestStimuliListenerUpdate();
#if ENABLE_DRAW_DEBUG
	if (bDrawSightDebug)
	{
		GetWorldTimerManager().SetTimer(SightDebugTimer, this, &ThisClass::DrawSightDebug, FMath::Max(0.05f, SightDebugInterval), true);
	}
#endif
}

void ATBMonsterAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(SightDebugTimer);
	GetWorldTimerManager().ClearTimer(TargetMemoryTimer);
	ClearTrackedTarget();
	if (ATBMonster* Monster = Cast<ATBMonster>(GetPawn()))
	{
		Monster->OnMonsterDied.RemoveDynamic(this, &ThisClass::HandleMonsterDied);
	}
	Super::EndPlay(EndPlayReason);
}

void ATBMonsterAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (!HasAuthority()) return;
	if (ATBMonster* Monster = Cast<ATBMonster>(InPawn))
	{
		Monster->OnMonsterDied.AddUniqueDynamic(this, &ThisClass::HandleMonsterDied);
		if (Monster->IsDead())
		{
			HandleMonsterDied(Monster);
			return;
		}
	}
	if (UAIPerceptionComponent* Perception = GetPerceptionComponent())
	{
		Perception->SetSenseEnabled(UAISense_Sight::StaticClass(), true);
	}
#if ENABLE_DRAW_DEBUG
	if (HasActorBegunPlay() && bDrawSightDebug)
	{
		GetWorldTimerManager().SetTimer(SightDebugTimer, this, &ThisClass::DrawSightDebug, FMath::Max(0.05f, SightDebugInterval), true);
	}
#endif
}

void ATBMonsterAIController::OnUnPossess()
{
	if (ATBMonster* Monster = Cast<ATBMonster>(GetPawn()))
	{
		Monster->OnMonsterDied.RemoveDynamic(this, &ThisClass::HandleMonsterDied);
	}
	GetWorldTimerManager().ClearTimer(TargetMemoryTimer);
	GetWorldTimerManager().ClearTimer(SightDebugTimer);
	ClearTrackedTarget();
	if (UBlackboardComponent* BlackboardComponent = GetBlackboardComponent()) BlackboardComponent->ClearValue(TEXT("TargetActor"));
	Super::OnUnPossess();
}

void ATBMonsterAIController::HandleMonsterDied(ATBMonster* Monster)
{
	if (!HasAuthority() || Monster != GetPawn()) return;
	GetWorldTimerManager().ClearTimer(TargetMemoryTimer);
	GetWorldTimerManager().ClearTimer(SightDebugTimer);
	ClearTrackedTarget();
	if (UBlackboardComponent* BlackboardComponent = GetBlackboardComponent()) BlackboardComponent->ClearValue(TEXT("TargetActor"));
	if (UAIPerceptionComponent* Perception = GetPerceptionComponent())
	{
		Perception->SetSenseEnabled(UAISense_Sight::StaticClass(), false);
		Perception->ForgetAll();
	}
}

void ATBMonsterAIController::DrawSightDebug()
{
#if ENABLE_DRAW_DEBUG
	APawn* Monster = GetPawn();
	UWorld* World = GetWorld();
	if (!bDrawSightDebug || !IsValid(Monster) || !World || !GetPerceptionComponent()) return;

	FVector EyeLocation;
	FRotator EyeRotation;
	GetActorEyesViewPoint(EyeLocation, EyeRotation);
	const float Lifetime = FMath::Max(0.05f, SightDebugInterval) * 1.5f;
	const FVector Forward = EyeRotation.Vector();
	const FVector Left = FRotator(0.0f, -PeripheralVisionAngle, 0.0f).RotateVector(Forward);
	const FVector Right = FRotator(0.0f, PeripheralVisionAngle, 0.0f).RotateVector(Forward);
	DrawDebugLine(World, EyeLocation, EyeLocation + Left * SightRadius, FColor::Yellow, false, Lifetime, 0, 1.5f);
	DrawDebugLine(World, EyeLocation, EyeLocation + Right * SightRadius, FColor::Yellow, false, Lifetime, 0, 1.5f);

	TArray<AActor*> SeenActors;
	GetPerceptionComponent()->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), SeenActors);
	for (TActorIterator<ATBPlayer> It(World); It; ++It)
	{
		ATBPlayer* Player = *It;
		if (!IsValid(Player) || FVector::DistSquared(EyeLocation, Player->GetActorLocation()) > FMath::Square(LoseSightRadius)) continue;
		const bool bSeen = SeenActors.Contains(Player);
		DrawDebugLine(World, EyeLocation, Player->GetActorLocation(), bSeen ? FColor::Green : FColor::Red, false, Lifetime, 0, 2.0f);
	}
#endif
}

void ATBMonsterAIController::HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!HasAuthority() || !Cast<ATBPlayer>(Actor) || Stimulus.Type != UAISense::GetSenseID<UAISense_Sight>())
	{
		return;
	}

	UpdateTarget();
}

void ATBMonsterAIController::HandleTargetDestroyed(AActor* DestroyedActor)
{
	UpdateTarget(DestroyedActor, true);
}

void ATBMonsterAIController::HandleTargetMemoryExpired()
{
	UpdateTarget(nullptr, true);
}

void ATBMonsterAIController::ClearTrackedTarget()
{
	if (AActor* PreviousTarget = TrackedTarget.Get())
	{
		PreviousTarget->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleTargetDestroyed);
	}
	TrackedTarget.Reset();
}

void ATBMonsterAIController::UpdateTarget(AActor* ExcludedActor, bool bMemoryExpired)
{
	if (!HasAuthority() || !GetPawn() || !GetPerceptionComponent())
	{
		return;
	}
	if (const ATBMonster* Monster = Cast<ATBMonster>(GetPawn()); Monster && Monster->IsDead()) return;

	UBlackboardComponent* BlackboardComponent = GetBlackboardComponent();
	if (!BlackboardComponent)
	{
		return;
	}

	TArray<AActor*> PerceivedActors;
	GetPerceptionComponent()->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), PerceivedActors);
	AActor* ClosestPlayer = nullptr;
	double ClosestDistanceSquared = TNumericLimits<double>::Max();
	for (AActor* Candidate : PerceivedActors)
	{
		if (!IsValid(Candidate) || Candidate == ExcludedActor || !Candidate->IsA<ATBPlayer>())
		{
			continue;
		}

		const double DistanceSquared = FVector::DistSquared(GetPawn()->GetActorLocation(), Candidate->GetActorLocation());
		if (DistanceSquared < ClosestDistanceSquared)
		{
			ClosestDistanceSquared = DistanceSquared;
			ClosestPlayer = Candidate;
		}
	}

	if (!ClosestPlayer && !bMemoryExpired && TrackedTarget.IsValid() && TrackedTarget.Get() != ExcludedActor && LostSightMemorySeconds > 0.0f)
	{
		if (!GetWorldTimerManager().IsTimerActive(TargetMemoryTimer))
		{
			GetWorldTimerManager().SetTimer(TargetMemoryTimer, this, &ThisClass::HandleTargetMemoryExpired, LostSightMemorySeconds, false);
		}
		return;
	}

	GetWorldTimerManager().ClearTimer(TargetMemoryTimer);
	if (TrackedTarget.Get() != ClosestPlayer)
	{
		ClearTrackedTarget();
		TrackedTarget = ClosestPlayer;
		if (ClosestPlayer)
		{
			ClosestPlayer->OnDestroyed.AddUniqueDynamic(this, &ThisClass::HandleTargetDestroyed);
		}
	}
	BlackboardComponent->SetValueAsObject(TEXT("TargetActor"), ClosestPlayer);
}
