#include "AI/AIControler/Monster/TBMonsterAIController.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "TBGameplayTags.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BrainComponent.h"
#include "Character/Player/TBPlayer.h"
#include "Character/Monster/TBMonster.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Navigation/PathFollowingComponent.h"

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
	PendingInvestigationLocation.Reset();
	FinishInvestigation(true);
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
	PendingInvestigationLocation.Reset();
	FinishInvestigation(true);
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

void ATBMonsterAIController::ReactivateFromPool()
{
	// 재사용 시 기존 감지 정보를 지우고 시야 및 AI Brain을 다시 시작합니다.
	if (!HasAuthority()) return;
	if (UAIPerceptionComponent* Perception = GetPerceptionComponent())
	{
		Perception->ForgetAll();
		Perception->SetSenseEnabled(UAISense_Sight::StaticClass(), true);
		Perception->RequestStimuliListenerUpdate();
	}
	if (UBrainComponent* Brain = GetBrainComponent()) Brain->RestartLogic();
#if ENABLE_DRAW_DEBUG
	if (bDrawSightDebug) GetWorldTimerManager().SetTimer(SightDebugTimer, this, &ThisClass::DrawSightDebug, FMath::Max(0.05f, SightDebugInterval), true);
#endif
}
void ATBMonsterAIController::InvestigateDamageSource(const AActor* DamageSource)
{
	const ATBMonster* Monster = Cast<ATBMonster>(GetPawn());
	const UBlackboardComponent* BlackboardComponent = GetBlackboardComponent();
	if (!HasAuthority() || !IsValid(Monster) || Monster->IsDead() || !IsValid(DamageSource) || DamageSource == Monster || !BlackboardComponent) return;
	if (BlackboardComponent->GetValueAsObject(TEXT("TargetActor"))) return;

	PendingInvestigationLocation = DamageSource->GetActorLocation();
	TryStartPendingInvestigation();
}

void ATBMonsterAIController::TryStartPendingInvestigation()
{
	ATBMonster* Monster = Cast<ATBMonster>(GetPawn());
	UBlackboardComponent* BlackboardComponent = GetBlackboardComponent();
	UBrainComponent* Brain = GetBrainComponent();
	if (!HasAuthority() || !PendingInvestigationLocation.IsSet() || !IsValid(Monster) || Monster->IsDead() || !BlackboardComponent) return;
	if (BlackboardComponent->GetValueAsObject(TEXT("TargetActor")))
	{
		PendingInvestigationLocation.Reset();
		return;
	}
	if (Monster->IsMovementBlocked() || (Brain && Brain->IsPaused() && !bPausedBrainForInvestigation)) return;

	const FVector Location = PendingInvestigationLocation.GetValue();
	PendingInvestigationLocation.Reset();
	FinishInvestigation(true);
	if (Brain && Brain->IsRunning() && !Brain->IsPaused())
	{
		Brain->PauseLogic(TEXT("Investigating damage source"));
		bPausedBrainForInvestigation = true;
	}
	FAIMoveRequest Request(Location);
	Request.SetAcceptanceRadius(150.0f);
	Request.SetUsePathfinding(true);
	Request.SetProjectGoalLocation(true);
	const FPathFollowingRequestResult MoveResult = MoveTo(Request);
	if (MoveResult.Code == EPathFollowingRequestResult::RequestSuccessful)
	{
		InvestigationRequestID = MoveResult.MoveId;
	}
	else
	{
		FinishInvestigation(false);
	}
}

void ATBMonsterAIController::FinishInvestigation(bool bStopMove)
{
	const bool bHadMove = InvestigationRequestID.IsValid();
	InvestigationRequestID = FAIRequestID::InvalidRequest;
	if (bStopMove && bHadMove) StopMovement();
	if (bPausedBrainForInvestigation)
	{
		bPausedBrainForInvestigation = false;
		const ATBMonster* Monster = Cast<ATBMonster>(GetPawn());
		if (IsValid(Monster) && !Monster->IsDead())
		{
			if (UBrainComponent* Brain = GetBrainComponent()) Brain->ResumeLogic(TEXT("Damage investigation finished"));
		}
	}
}

void ATBMonsterAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);
	if (InvestigationRequestID.IsValid() && RequestID == InvestigationRequestID) FinishInvestigation(false);
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
	if (!HasAuthority() || Stimulus.Type != UAISense::GetSenseID<UAISense_Sight>())
	{
		return;
	}

	UpdateTarget();
}

void ATBMonsterAIController::HandleTargetDestroyed(AActor* DestroyedActor)
{
	UpdateTarget(DestroyedActor, true);
}

void ATBMonsterAIController::HandleTargetDeadTagChanged(FGameplayTag Tag, int32 NewCount)
{
	// 사망 태그가 추가되면 현재 타깃을 제외하고 재선택
	if (NewCount > 0) UpdateTarget(TrackedTarget.Get(), true);
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
	if (UAbilitySystemComponent* ASC = TrackedTargetASC.Get())
	{
		ASC->UnregisterGameplayTagEvent(TargetDeadTagHandle, TBGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved);
	}
	TargetDeadTagHandle.Reset();
	TrackedTargetASC.Reset();
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
		UAbilitySystemComponent* ASC = IsValid(Candidate) ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Candidate) : nullptr;
		if (!IsValid(ASC) || Candidate == ExcludedActor || ASC->GetAvatarActor() != Candidate
			|| (!ASC->HasMatchingGameplayTag(TBGameplayTags::Character_Type_Player)
				&& !ASC->HasMatchingGameplayTag(TBGameplayTags::Character_Type_Human))
			|| ASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead))
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

	AActor* PreviousTarget = TrackedTarget.Get();
	UAbilitySystemComponent* PreviousASC = TrackedTargetASC.Get();
	if (!ClosestPlayer && !bMemoryExpired && IsValid(PreviousTarget) && IsValid(PreviousASC)
		&& PreviousASC->GetAvatarActor() == PreviousTarget
		&& (PreviousASC->HasMatchingGameplayTag(TBGameplayTags::Character_Type_Player)
			|| PreviousASC->HasMatchingGameplayTag(TBGameplayTags::Character_Type_Human))
		&& !PreviousASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead) && PreviousTarget != ExcludedActor && LostSightMemorySeconds > 0.0f)
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
			UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(ClosestPlayer);
			TrackedTargetASC = ASC;
			TargetDeadTagHandle = ASC->RegisterGameplayTagEvent(TBGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleTargetDeadTagChanged);
		}
	}
	BlackboardComponent->SetValueAsObject(TEXT("TargetActor"), ClosestPlayer);
	if (ClosestPlayer)
	{
		PendingInvestigationLocation.Reset();
		FinishInvestigation(true);
	}
}
