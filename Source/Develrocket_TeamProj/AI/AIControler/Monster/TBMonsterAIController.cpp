#include "AI/AIControler/Monster/TBMonsterAIController.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "TBGameplayTags.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BrainComponent.h"
#include "Character/Player/TBPlayer.h"
#include "Character/Monster/TBMonster.h"
#include "Components/TBMonsterMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Character/Human/TBCitizen.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "GameFramework/CharacterMovementComponent.h"

ATBMonsterAIController::ATBMonsterAIController()
{
	bPreferPatrolHeading = true;
	bSkipBlockedPatrolPoint = true;
	UAIPerceptionComponent* PerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComp"));
	SetPerceptionComponent(* PerceptionComp);
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	PerceptionComp->ConfigureSense(*SightConfig);
	PerceptionComp->SetDominantSense(UAISense_Sight::StaticClass());
}

ETeamAttitude::Type ATBMonsterAIController::GetTeamAttitudeTowards(const AActor& Other) const
{
	if (CanTargetActor(Other)) return ETeamAttitude::Hostile;
	if (Other.IsA<ATBMonster>()) return ETeamAttitude::Friendly;
	return ETeamAttitude::Neutral;
}

bool ATBMonsterAIController::CanTargetActor(const AActor& Actor) const
{
	return Actor.IsA<ATBPlayer>() || Actor.IsA<ATBCitizen>();
}

bool ATBMonsterAIController::RunBehaviorTree(UBehaviorTree* BTAsset)
{
	if (const ATBMonster* Monster = Cast<ATBMonster>(GetPawn()))
	{
		if (UBehaviorTree* Override = Monster->GetBehaviorTreeOverride())
		{
			const UBehaviorTreeComponent* Tree = Cast<UBehaviorTreeComponent>(GetBrainComponent());
			return (Tree && Tree->IsRunning() && Tree->GetRootTree() == Override)
				|| Super::RunBehaviorTree(Override);
		}
	}
	return Super::RunBehaviorTree(BTAsset);
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
	SightConfig->DetectionByAffiliation.bDetectNeutrals = false;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = false;
	
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
		UPathFollowingComponent* PathFollowing = GetPathFollowingComponent();
		if (UTBMonsterMovementComponent* Movement = Cast<UTBMonsterMovementComponent>(Monster->GetCharacterMovement());
			PathFollowing && Movement)
		{
			PathFollowing->PostProcessMove.BindUObject(Movement, &UTBMonsterMovementComponent::SteerAroundPathObstacle);
		}
		Monster->OnMonsterDied.AddUniqueDynamic(this, &ThisClass::HandleMonsterDied);
		if (Monster->IsDead())
		{
			HandleMonsterDied(Monster);
			return;
		}
		if (UBehaviorTree* Override = Monster->GetBehaviorTreeOverride())
		{
			RunBehaviorTree(Override);
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
	NextCombatRepositionTime = 0.0;
	if (UPathFollowingComponent* PathFollowing = GetPathFollowingComponent())
	{
		PathFollowing->PostProcessMove.Unbind();
	}
	bPoolSuspended = false;
	bPreserveTargetDuringStagger = false;
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
	DeactivateForPool();
}

void ATBMonsterAIController::DeactivateForPool()
{
	if (!HasAuthority() || bPoolSuspended) return;
	bPoolSuspended = true;
	NextCombatRepositionTime = 0.0;
	bPausedBrainForInvestigation = false;
	if (UBrainComponent* Brain = GetBrainComponent()) Brain->StopLogic(TEXT("Monster inactive"));
	StopMovement();
	ResetPatrolState();
	bPreserveTargetDuringStagger = false;
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
	if (!HasAuthority() || !bPoolSuspended) return;
	bPoolSuspended = false;
	if (UAIPerceptionComponent* Perception = GetPerceptionComponent())
	{
		Perception->ForgetAll();
		Perception->SetSenseEnabled(UAISense_Sight::StaticClass(), true);
		Perception->RequestStimuliListenerUpdate();
	}
	if (UBrainComponent* Brain = GetBrainComponent())
	{
		if (Brain->IsPaused()) Brain->ResumeLogic(TEXT("Monster reactivated"));
		Brain->RestartLogic();
	}
#if ENABLE_DRAW_DEBUG
	if (bDrawSightDebug) GetWorldTimerManager().SetTimer(SightDebugTimer, this, &ThisClass::DrawSightDebug, FMath::Max(0.05f, SightDebugInterval), true);
#endif
}
void ATBMonsterAIController::InvestigateDamageSource(AActor* DamageSource)
{
	const ATBMonster* Monster = Cast<ATBMonster>(GetPawn());
	UBlackboardComponent* BlackboardComponent = GetBlackboardComponent();
	if (!HasAuthority() || bPoolSuspended || !IsValid(Monster) || Monster->IsDead() || !IsValid(DamageSource) || DamageSource == Monster || !BlackboardComponent) return;
	UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(DamageSource);
	if (IsValid(SourceASC) && SourceASC->GetAvatarActor() == DamageSource
		&& !SourceASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead)
		&& CanTargetActor(*DamageSource))
	{
		if (!bDamagePriorityTarget || !TrackedTarget.IsValid() || !TrackedTargetASC.IsValid()
			|| TrackedTargetASC->GetAvatarActor() != TrackedTarget.Get()
			|| TrackedTargetASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead))
		{
			SetTrackedTarget(DamageSource);
			bDamagePriorityTarget = true;
		}
		if (TrackedTarget.Get() == DamageSource)
		{
			GetWorldTimerManager().ClearTimer(TargetMemoryTimer);
			BlackboardComponent->SetValueAsObject(TEXT("TargetActor"), TrackedTarget.Get());
			PendingInvestigationLocation.Reset();
			FinishInvestigation(true);
			UpdateTarget();
		}
		return;
	}
	if (BlackboardComponent->GetValueAsObject(TEXT("TargetActor"))) return;

	PendingInvestigationLocation = DamageSource->GetActorLocation();
	TryStartPendingInvestigation();
}

void ATBMonsterAIController::TryStartPendingInvestigation()
{
	ATBMonster* Monster = Cast<ATBMonster>(GetPawn());
	UBlackboardComponent* BlackboardComponent = GetBlackboardComponent();
	UBrainComponent* Brain = GetBrainComponent();
	if (!HasAuthority() || bPoolSuspended || !PendingInvestigationLocation.IsSet() || !IsValid(Monster) || Monster->IsDead() || !BlackboardComponent) return;
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

bool ATBMonsterAIController::WantsCombatRetreat(const AActor& Target) const
{
	const ATBMonster* Monster = Cast<ATBMonster>(GetPawn());
	return Monster && Monster->GetMonsterBehavior() == ETBMonsterBehavior::SpideEye
		&& Target.IsA<ATBPlayer>() && GetWorld()->GetTimeSeconds() >= NextCombatRepositionTime
		&& FVector::DistSquared2D(Monster->GetActorLocation(), Target.GetActorLocation()) < FMath::Square(260.0)
		&& LineOfSightTo(&Target);
}

FPathFollowingRequestResult ATBMonsterAIController::MoveTo(const FAIMoveRequest& MoveRequest, FNavPathSharedPtr* OutPath)
{
	const AActor* Target = MoveRequest.IsMoveToActorRequest() ? MoveRequest.GetGoalActor() : nullptr;
	ATBMonster* Monster = Cast<ATBMonster>(GetPawn());
	if (!HasAuthority() || !IsValid(Monster) || !IsValid(Target) || Target != TrackedTarget.Get()
		|| Monster->IsMovementBlocked())
	{
		return Super::MoveTo(MoveRequest, OutPath);
	}

	if (Monster->IsSummonedMonster() && Monster->GetMeleeAttackRange() > 0.0f)
	{
		// Leave room to brake and keep the target within the scaled melee trace.
		const float ApproachDistance = FMath::Max(Monster->GetSimpleCollisionRadius() + 5.0f,
			Monster->GetMeleeAttackRange() * 0.5f);
		FAIMoveRequest ChaseRequest(MoveRequest);
		ChaseRequest.SetAcceptanceRadius(Target->GetSimpleCollisionRadius() + ApproachDistance)
			.SetReachTestIncludesAgentRadius(false).SetReachTestIncludesGoalRadius(false);
		return Super::MoveTo(ChaseRequest, OutPath);
	}

	if (GetWorld()->GetTimeSeconds() < NextCombatRepositionTime)
	{
		return Super::MoveTo(MoveRequest, OutPath);
	}

	const FVector Origin = GetNavAgentLocation();
	const FVector Offset = Target->GetActorLocation() - Origin;
	const double Distance = Offset.Size2D();
	const FVector TowardTarget = Offset.GetSafeNormal2D();
	const FVector Sideways = FVector::CrossProduct(FVector::UpVector, TowardTarget);
	const ETBMonsterBehavior Behavior = Monster->GetMonsterBehavior();
	const bool bFlank = Behavior == ETBMonsterBehavior::Anasimus && Distance > Monster->GetMeleeAttackRange()
		&& Distance < 850.0 && FVector::DotProduct(Target->GetActorForwardVector().GetSafeNormal2D(), -TowardTarget) > 0.45;
	const bool bRetreat = WantsCombatRetreat(*Target);
	if ((!bFlank && !bRetreat) || TowardTarget.IsNearlyZero() || (bFlank && !LineOfSightTo(Target)))
	{
		return Super::MoveTo(MoveRequest, OutPath);
	}

	const FVector ForwardOffset = TowardTarget * (bFlank ? FMath::Min(220.0, Distance * 0.5) : -180.0);
	const FVector LateralOffset = Sideways * (bFlank ? 270.0 : 130.0) * (FMath::RandBool() ? 1.0 : -1.0);
	const UNavigationSystemV1* Navigation = UNavigationSystemV1::GetCurrent(GetWorld());
	const ANavigationData* NavData = Navigation ? Navigation->GetNavDataForProps(GetNavAgentPropertiesRef(), Origin) : nullptr;
	NextCombatRepositionTime = GetWorld()->GetTimeSeconds() + (bFlank ? 4.0 : 2.5);

	for (const double Side : {1.0, -1.0})
	{
		FNavLocation Goal;
		if (!NavData || !Navigation->ProjectPointToNavigation(Origin + ForwardOffset + LateralOffset * Side, Goal,
			FVector(60.0, 60.0, 150.0), NavData)
			|| FVector::DistSquared2D(Origin, Goal.Location) < FMath::Square(90.0))
		{
			continue;
		}

		FAIMoveRequest SideMove(Goal.Location);
		SideMove.SetAcceptanceRadius(35.0f).SetAllowPartialPath(false).SetReachTestIncludesAgentRadius(false)
			.SetCanStrafe(MoveRequest.CanStrafe()).SetNavigationFilter(MoveRequest.GetNavigationFilter());
		FPathFindingQuery Query;
		FNavPathSharedPtr Path;
		if (!BuildPathfindingQuery(SideMove, Query)) continue;
		FindPathForMoveRequest(SideMove, Query, Path);
		if (!Path.IsValid() || !Path->IsValid() || Path->IsPartial()) continue;

		FPathFollowingRequestResult Result;
		Result.MoveId = RequestMove(SideMove, Path);
		if (Result.MoveId.IsValid())
		{
			bAllowStrafe = SideMove.CanStrafe();
			Result.Code = EPathFollowingRequestResult::RequestSuccessful;
			if (OutPath) *OutPath = Path;
			return Result;
		}
	}
	return Super::MoveTo(MoveRequest, OutPath);
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

void ATBMonsterAIController::PreserveTargetDuringStagger()
{
	if (!HasAuthority()) return;
	bPreserveTargetDuringStagger = true;
	GetWorldTimerManager().ClearTimer(TargetMemoryTimer);
}

void ATBMonsterAIController::RefreshTargetAfterStagger()
{
	if (!HasAuthority()) return;
	bPreserveTargetDuringStagger = false;
	AActor* Target = TrackedTarget.Get();
	UAbilitySystemComponent* ASC = TrackedTargetASC.Get();
	UBlackboardComponent* BlackboardComponent = GetBlackboardComponent();
	if (!IsValid(Target) || !IsValid(ASC) || ASC->GetAvatarActor() != Target
		|| ASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead)
		|| !CanTargetActor(*Target) || !BlackboardComponent || !GetPerceptionComponent())
	{
		UpdateTarget();
		return;
	}

	BlackboardComponent->SetValueAsObject(TEXT("TargetActor"), Target);
	TArray<AActor*> PerceivedActors;
	GetPerceptionComponent()->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), PerceivedActors);
	GetWorldTimerManager().ClearTimer(TargetMemoryTimer);
	if (!PerceivedActors.Contains(Target))
	{
		GetWorldTimerManager().SetTimer(TargetMemoryTimer, this, &ThisClass::HandleTargetMemoryExpired,
			FMath::Max(0.1f, LostSightMemorySeconds), false);
	}
}

void ATBMonsterAIController::ClearTrackedTarget()
{
	bDamagePriorityTarget = false;
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

void ATBMonsterAIController::SetTrackedTarget(AActor* Target)
{
	if (TrackedTarget.Get() == Target) return;
	ClearTrackedTarget();
	TrackedTarget = Target;
	if (IsValid(Target))
	{
		Target->OnDestroyed.AddUniqueDynamic(this, &ThisClass::HandleTargetDestroyed);
		UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
		TrackedTargetASC = ASC;
		if (IsValid(ASC)) TargetDeadTagHandle = ASC->RegisterGameplayTagEvent(TBGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleTargetDeadTagChanged);
	}
}

void ATBMonsterAIController::UpdateTarget(AActor* ExcludedActor, bool bMemoryExpired)
{
	if (!HasAuthority() || bPoolSuspended || !GetPawn() || !GetPerceptionComponent())
	{
		return;
	}
	if (const ATBMonster* Monster = Cast<ATBMonster>(GetPawn()); Monster && Monster->IsDead()) return;

	UBlackboardComponent* BlackboardComponent = GetBlackboardComponent();
	if (!BlackboardComponent)
	{
		return;
	}
	// Stagger 또는 피격 우선 추적 중에는 유효한 기존 대상을 유지한다.
	AActor* PreservedTarget = TrackedTarget.Get();
	UAbilitySystemComponent* PreservedASC = TrackedTargetASC.Get();
	if ((bPreserveTargetDuringStagger || (bDamagePriorityTarget && !bMemoryExpired)) && IsValid(PreservedTarget) && PreservedTarget != ExcludedActor
		&& IsValid(PreservedASC) && PreservedASC->GetAvatarActor() == PreservedTarget
		&& !PreservedASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead)
		&& CanTargetActor(*PreservedTarget))
	{
		TArray<AActor*> SeenActors;
		GetPerceptionComponent()->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), SeenActors);
		if (bPreserveTargetDuringStagger || SeenActors.Contains(PreservedTarget))
		{
			GetWorldTimerManager().ClearTimer(TargetMemoryTimer);
		}
		else if (!GetWorldTimerManager().IsTimerActive(TargetMemoryTimer))
		{
			GetWorldTimerManager().SetTimer(TargetMemoryTimer, this, &ThisClass::HandleTargetMemoryExpired, FMath::Max(0.1f, LostSightMemorySeconds), false);
		}
		BlackboardComponent->SetValueAsObject(TEXT("TargetActor"), PreservedTarget);
		return;
	}
	if (bDamagePriorityTarget) bDamagePriorityTarget = false;

	TArray<AActor*> PerceivedActors;
	GetPerceptionComponent()->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), PerceivedActors);
	AActor* ClosestTarget = nullptr;
	double ClosestDistanceSquared = TNumericLimits<double>::Max();
	for (AActor* Candidate : PerceivedActors)
	{
		UAbilitySystemComponent* ASC = IsValid(Candidate) ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Candidate) : nullptr;
		if (!IsValid(ASC) || Candidate == ExcludedActor || ASC->GetAvatarActor() != Candidate
			|| !CanTargetActor(*Candidate)
			|| ASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead))
		{
			continue;
		}

		const double DistanceSquared = FVector::DistSquared(GetPawn()->GetActorLocation(), Candidate->GetActorLocation());
		if (DistanceSquared < ClosestDistanceSquared)
		{
			ClosestDistanceSquared = DistanceSquared;
			ClosestTarget = Candidate;
		}
	}

	AActor* PreviousTarget = TrackedTarget.Get();
	UAbilitySystemComponent* PreviousASC = TrackedTargetASC.Get();
	if (!ClosestTarget && !bMemoryExpired && IsValid(PreviousTarget) && IsValid(PreviousASC)
		&& PreviousASC->GetAvatarActor() == PreviousTarget
		&& CanTargetActor(*PreviousTarget)
		&& !PreviousASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead) && PreviousTarget != ExcludedActor && LostSightMemorySeconds > 0.0f)
	{
		if (!GetWorldTimerManager().IsTimerActive(TargetMemoryTimer))
		{
			GetWorldTimerManager().SetTimer(TargetMemoryTimer, this, &ThisClass::HandleTargetMemoryExpired, LostSightMemorySeconds, false);
		}
		return;
	}

	GetWorldTimerManager().ClearTimer(TargetMemoryTimer);
	SetTrackedTarget(ClosestTarget);
	BlackboardComponent->SetValueAsObject(TEXT("TargetActor"), ClosestTarget);
	if (ClosestTarget)
	{
		PendingInvestigationLocation.Reset();
		FinishInvestigation(true);
	}
}
