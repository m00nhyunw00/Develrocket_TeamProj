#include "AI/TBMonsterAIController.h"

#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/Player/TBPlayer.h"

ATBMonsterAIController::ATBMonsterAIController()
{
	UAIPerceptionComponent* PerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComp"));
	SetPerceptionComponent(* PerceptionComp);
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
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
	UpdateTarget(DestroyedActor);
}

void ATBMonsterAIController::UpdateTarget(AActor* ExcludedActor)
{
	if (!HasAuthority() || !GetPawn() || !GetPerceptionComponent())
	{
		return;
	}

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

	if (TrackedTarget.Get() != ClosestPlayer)
	{
		if (AActor* PreviousTarget = TrackedTarget.Get())
		{
			PreviousTarget->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleTargetDestroyed);
		}
		TrackedTarget = ClosestPlayer;
		if (ClosestPlayer)
		{
			ClosestPlayer->OnDestroyed.AddUniqueDynamic(this, &ThisClass::HandleTargetDestroyed);
		}
	}
	BlackboardComponent->SetValueAsObject(TEXT("TargetActor"), ClosestPlayer);
}
