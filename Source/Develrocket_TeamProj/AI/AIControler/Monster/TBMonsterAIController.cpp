#include "AI/AIControler/Monster/TBMonsterAIController.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "TBGameplayTags.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "Tasks/AITask_MoveTo.h"
#include "AI/TBCombatTokenSubsystem.h"
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

namespace
{
	constexpr EAIFocusPriority::Type AnasimusCombatFocus = EAIFocusPriority::Gameplay + 1;
	constexpr EAIFocusPriority::Type CombatWaitingFocus = EAIFocusPriority::Gameplay + 2;
	constexpr double WaitingTargetReplanDistance = 120.0;
	constexpr int32 WaitingSlotsPerRing = 8;
	constexpr int32 WaitingRingCount = 2;

	double GetCombatWaitingRadius(const ATBMonster& Monster, const AActor& Target)
	{
		const double ContactDistance = Monster.GetSimpleCollisionRadius() + Target.GetSimpleCollisionRadius();
		const double SafeRadius = FMath::Max3(260.0, ContactDistance + 105.0, Monster.GetMeleeAttackRange() + 75.0);
		return SafeRadius + Monster.GetCombatWaitingExtraDistance();
	}
}

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
		if (UTBCombatTokenSubsystem* Tokens = GetWorld()->GetSubsystem<UTBCombatTokenSubsystem>()) Tokens->ClearAttacker(*Monster);
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
		if (UTBCombatTokenSubsystem* Tokens = GetWorld()->GetSubsystem<UTBCombatTokenSubsystem>()) Tokens->ClearAttacker(*Monster);
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
	if (ATBMonster* Monster = Cast<ATBMonster>(GetPawn()))
	{
		if (UTBCombatTokenSubsystem* Tokens = GetWorld()->GetSubsystem<UTBCombatTokenSubsystem>()) Tokens->ClearAttacker(*Monster);
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

bool ATBMonsterAIController::ShouldUseCombatWaitingMove(const ATBMonster& Monster, const AActor& Target) const
{
	const UTBCombatTokenSubsystem* Tokens = GetWorld()->GetSubsystem<UTBCombatTokenSubsystem>();
	const double WaitingDistance = GetCombatWaitingRadius(Monster, Target) + 290.0;
	return Tokens && Tokens->IsAttackFull(Target) && Monster.GetMonsterBehavior() != ETBMonsterBehavior::Hunter
		&& FVector::DistSquared2D(Monster.GetActorLocation(), Target.GetActorLocation()) <= FMath::Square(WaitingDistance)
		&& LineOfSightTo(&Target);
}

void ATBMonsterAIController::RefreshCombatPursuit(UBehaviorTreeComponent& Tree, const UBTTask_MoveTo& MoveNode)
{
	ATBMonster* Monster = Cast<ATBMonster>(GetPawn());
	AActor* Target = TrackedTarget.Get();
	if (!IsValid(Monster) || Monster->IsDead() || Monster->IsMovementBlocked() || !IsValid(Target)
		|| MoveNode.GetSelectedBlackboardKey() != TEXT("TargetActor")
		|| !Tree.GetBlackboardComponent() || Tree.GetBlackboardComponent()->GetValueAsObject(TEXT("TargetActor")) != Target) return;

	const bool bShouldWait = ShouldUseCombatWaitingMove(*Monster, *Target);
	if (bShouldWait)
	{
		if (GetWorld()->GetTimeSeconds() < NextWaitingPlanTime) return;
		if (CombatWaitingMoveID.IsValid() && FVector::DistSquared2D(WaitingPlanTargetLocation, Target->GetActorLocation())
			< FMath::Square(WaitingTargetReplanDistance)) return;
	}
	else if (!bCombatStandOff && !CombatWaitingMoveID.IsValid() && CombatWaitingSlot == INDEX_NONE)
	{
		return;
	}

	// Replan through the existing gameplay task so the BT still owns and observes the move.
	const auto* Memory = reinterpret_cast<const FBTMoveToTaskMemory*>(Tree.GetNodeMemory(&MoveNode, Tree.GetActiveInstanceIdx()));
	if (UAITask_MoveTo* MoveTask = Memory ? Memory->Task.Get() : nullptr; MoveTask && MoveTask->IsActive())
	{
		MoveTask->GetMoveRequestRef().SetGoalActor(Target);
		MoveTask->ConditionalPerformMove();
	}
}

FPathFollowingRequestResult ATBMonsterAIController::MoveAroundCombatTarget(const FAIMoveRequest& MoveRequest,
	ATBMonster& Monster, AActor& Target, FNavPathSharedPtr* OutPath)
{
	const FVector Origin = GetNavAgentLocation();
	const FVector Center = Target.GetActorLocation();
	const UNavigationSystemV1* Navigation = UNavigationSystemV1::GetCurrent(GetWorld());
	const ANavigationData* NavData = Navigation ? Navigation->GetNavDataForProps(GetNavAgentPropertiesRef(), Origin) : nullptr;
	UTBCombatTokenSubsystem* Tokens = GetWorld()->GetSubsystem<UTBCombatTokenSubsystem>();
	if (!NavData || !Tokens) return {};

	const bool bKeepCurrentSlot = CombatWaitingMoveID.IsValid();
	const double AngleStep = 360.0 / WaitingSlotsPerRing;
	const FVector Radial = (Origin - Center).GetSafeNormal2D();
	const int32 NearestAngle = FMath::RoundToInt(Radial.Rotation().Yaw / AngleStep);
	const int32 StartAngle = CombatWaitingSlot == INDEX_NONE ? NearestAngle : CombatWaitingSlot % WaitingSlotsPerRing;
	const int32 FirstOffset = CombatWaitingSlot != INDEX_NONE && !bKeepCurrentSlot ? 1 : 0;
	const double Radius = GetCombatWaitingRadius(Monster, Target);
	WaitingPlanTargetLocation = Center;
	for (int32 Ring = 0; Ring < WaitingRingCount; ++Ring)
	{
		for (int32 Step = 0; Step < WaitingSlotsPerRing; ++Step)
		{
			const int32 Angle = (StartAngle + (Step + FirstOffset) * WaitingOrbitDirection + WaitingSlotsPerRing * 2) % WaitingSlotsPerRing;
			const int32 Slot = Ring * WaitingSlotsPerRing + Angle;
			if (!Tokens->IsWaitingSlotAvailable(Target, Monster, Slot)) continue;

			const FVector Direction = FRotator(0.0, Angle * AngleStep, 0.0).Vector();
			if (FVector::DotProduct(Direction, Radial) < -0.01) continue;
			const FVector DesiredGoal = Center + Direction * (Radius + 150.0 * Ring);
			FNavLocation Goal;
			if (!Navigation->ProjectPointToNavigation(DesiredGoal, Goal, FVector(55.0, 55.0, 140.0), NavData)
				|| FVector::DistSquared2D(Goal.Location, Origin) < FMath::Square(75.0)) continue;

			FAIMoveRequest WaitMove(Goal.Location);
			WaitMove.SetAcceptanceRadius(36.0f).SetAllowPartialPath(false).SetCanStrafe(true).SetReachTestIncludesAgentRadius(false)
				.SetNavigationFilter(MoveRequest.GetNavigationFilter());
			FPathFindingQuery Query;
			FNavPathSharedPtr Path;
			if (!BuildPathfindingQuery(WaitMove, Query)) continue;
			FindPathForMoveRequest(WaitMove, Query, Path);
			if (!Path.IsValid() || !Path->IsValid() || Path->IsPartial()) continue;

			FPathFollowingRequestResult Result;
			Result.MoveId = RequestMove(WaitMove, Path);
			if (!Result.MoveId.IsValid()) continue;

			CombatWaitingMoveID = Result.MoveId;
			CombatWaitingSlot = Slot;
			Tokens->SetWaitingSlot(Target, Monster, Slot);
			UCharacterMovementComponent* Movement = Monster.GetCharacterMovement();
			bPreviousWaitingOrientToMovement = Movement->bOrientRotationToMovement;
			bPreviousWaitingUseControllerRotation = Movement->bUseControllerDesiredRotation;
			bPreviousWaitingUseControllerYaw = Monster.bUseControllerRotationYaw;
			bPreviousWaitingAllowStrafe = bAllowStrafe;
			Movement->bOrientRotationToMovement = false;
			Movement->bUseControllerDesiredRotation = true;
			Monster.bUseControllerRotationYaw = false;
			SetFocus(&Target, CombatWaitingFocus);
			bAllowStrafe = true;
			Result.Code = EPathFollowingRequestResult::RequestSuccessful;
			if (OutPath) *OutPath = Path;
			return Result;
		}
	}
	return {};
}

void ATBMonsterAIController::FinishCombatWaitingMove(bool bAbortMove)
{
	if (!CombatWaitingMoveID.IsValid()) return;
	const FAIRequestID FinishedID = CombatWaitingMoveID;
	CombatWaitingMoveID = FAIRequestID::InvalidRequest;
	ClearFocus(CombatWaitingFocus);
	bAllowStrafe = bPreviousWaitingAllowStrafe;
	if (ACharacter* ControlledCharacter = GetCharacter())
	{
		UCharacterMovementComponent* Movement = ControlledCharacter->GetCharacterMovement();
		Movement->bOrientRotationToMovement = bPreviousWaitingOrientToMovement;
		Movement->bUseControllerDesiredRotation = bPreviousWaitingUseControllerRotation;
		ControlledCharacter->bUseControllerRotationYaw = bPreviousWaitingUseControllerYaw;
	}
	if (bAbortMove && GetCurrentMoveRequestID() == FinishedID) StopMovement();
}

void ATBMonsterAIController::ClearCombatWaitingSlot()
{
	if (CombatWaitingSlot == INDEX_NONE) return;
	CombatWaitingSlot = INDEX_NONE;
	ATBMonster* Monster = Cast<ATBMonster>(GetPawn());
	AActor* Target = TrackedTarget.Get();
	if (UTBCombatTokenSubsystem* Tokens = GetWorld()->GetSubsystem<UTBCombatTokenSubsystem>(); Tokens && Target && Monster)
	{
		Tokens->ClearWaitingSlot(*Target, *Monster);
	}
}

FPathFollowingRequestResult ATBMonsterAIController::MoveTo(const FAIMoveRequest& MoveRequest, FNavPathSharedPtr* OutPath)
{
	AActor* Target = MoveRequest.IsMoveToActorRequest() ? MoveRequest.GetGoalActor() : nullptr;
	ATBMonster* Monster = Cast<ATBMonster>(GetPawn());
	if (!HasAuthority() || !IsValid(Monster) || !IsValid(Target) || Target != TrackedTarget.Get()
		|| Monster->IsMovementBlocked())
	{
		return Super::MoveTo(MoveRequest, OutPath);
	}

	if (ShouldUseCombatWaitingMove(*Monster, *Target))
	{
		bCombatStandOff = false;
		const double Now = GetWorld()->GetTimeSeconds();
		if (Now >= NextWaitingPlanTime)
		{
			NextWaitingPlanTime = Now + 0.45;
			const FPathFollowingRequestResult Result = MoveAroundCombatTarget(MoveRequest, *Monster, *Target, OutPath);
			if (Result.Code == EPathFollowingRequestResult::RequestSuccessful) return Result;
		}
		const float WaitingRadius = GetCombatWaitingRadius(*Monster, *Target);
		FAIMoveRequest StandOff(MoveRequest);
		StandOff.SetAcceptanceRadius(WaitingRadius).SetReachTestIncludesAgentRadius(false).SetReachTestIncludesGoalRadius(false);
		bCombatStandOff = true;
		return Super::MoveTo(StandOff, OutPath);
	}

	bCombatStandOff = false;
	FinishCombatWaitingMove(false);
	ClearCombatWaitingSlot();

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

	if (Monster->GetMonsterBehavior() == ETBMonsterBehavior::Anasimus)
	{
		return MoveAnasimusTowardTarget(MoveRequest, *Target, *Monster, OutPath);
	}

	if (GetWorld()->GetTimeSeconds() < NextCombatRepositionTime)
	{
		return Super::MoveTo(MoveRequest, OutPath);
	}

	const FVector Origin = GetNavAgentLocation();
	const FVector Offset = Target->GetActorLocation() - Origin;
	const FVector TowardTarget = Offset.GetSafeNormal2D();
	const FVector Sideways = FVector::CrossProduct(FVector::UpVector, TowardTarget);
	if (!WantsCombatRetreat(*Target) || TowardTarget.IsNearlyZero())
	{
		return Super::MoveTo(MoveRequest, OutPath);
	}

	const FVector ForwardOffset = TowardTarget * -180.0;
	const FVector LateralOffset = Sideways * 130.0 * (FMath::RandBool() ? 1.0 : -1.0);
	const UNavigationSystemV1* Navigation = UNavigationSystemV1::GetCurrent(GetWorld());
	const ANavigationData* NavData = Navigation ? Navigation->GetNavDataForProps(GetNavAgentPropertiesRef(), Origin) : nullptr;
	NextCombatRepositionTime = GetWorld()->GetTimeSeconds() + 2.5;

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

FPathFollowingRequestResult ATBMonsterAIController::MoveAnasimusTowardTarget(const FAIMoveRequest& MoveRequest,
	AActor& Target, ATBMonster& Monster, FNavPathSharedPtr* OutPath)
{
	FAIMoveRequest DirectMove(MoveRequest);
	const float ContactDistance = Monster.GetSimpleCollisionRadius() + Target.GetSimpleCollisionRadius() + 5.0f;
	const float ApproachDistance = FMath::Max(ContactDistance, Monster.GetMeleeAttackRange() - 20.0f);
	DirectMove.SetAcceptanceRadius(ApproachDistance).SetReachTestIncludesAgentRadius(false).SetReachTestIncludesGoalRadius(false);
	return Super::MoveTo(DirectMove, OutPath);
}

bool ATBMonsterAIController::TryAnasimusBackstep(AActor& Target)
{
	ATBMonster* Monster = Cast<ATBMonster>(GetPawn());
	if (!IsValid(Monster) || Monster->GetMonsterBehavior() != ETBMonsterBehavior::Anasimus
		|| TrackedTarget.Get() != &Target || Monster->IsMovementBlocked() || bPoolSuspended
		|| !Monster->GetCharacterMovement()->IsMovingOnGround())
	{
		return false;
	}

	const FVector Origin = GetNavAgentLocation();
	const FVector Away = (Origin - Target.GetActorLocation()).GetSafeNormal2D();
	const UNavigationSystemV1* Navigation = UNavigationSystemV1::GetCurrent(GetWorld());
	const ANavigationData* NavData = Navigation ? Navigation->GetNavDataForProps(GetNavAgentPropertiesRef(), Origin) : nullptr;
	if (!NavData || Away.IsNearlyZero()) return false;

	for (const double Distance : {170.0, 90.0})
	{
		FNavLocation Goal;
		FVector NavHit;
		if (!Navigation->ProjectPointToNavigation(Origin + Away * Distance, Goal, FVector(35.0, 35.0, 80.0), NavData)
			|| FVector::DotProduct(Goal.Location - Origin, Away) < 65.0
			|| NavData->Raycast(Origin, Goal.Location, NavHit, NavData->GetDefaultQueryFilter(), this))
		{
			continue;
		}

		FAIMoveRequest Backstep(Goal.Location);
		Backstep.SetAcceptanceRadius(12.0f);
		if (RequestAnasimusMove(Backstep, Target, nullptr).Code == EPathFollowingRequestResult::RequestSuccessful)
		{
			UCharacterMovementComponent* Movement = Monster->GetCharacterMovement();
			PreviousBackstepSpeed = Movement->MaxWalkSpeed;
			Movement->MaxWalkSpeed = FMath::Max(420.0f, Movement->MaxWalkSpeed * 1.8f);
			return true;
		}
	}
	return false;
}

FPathFollowingRequestResult ATBMonsterAIController::RequestAnasimusMove(FAIMoveRequest Request, AActor& Target,
	FNavPathSharedPtr* OutPath)
{
	Request.SetAllowPartialPath(false).SetCanStrafe(true)
		.SetReachTestIncludesAgentRadius(false).SetReachTestIncludesGoalRadius(false);
	FPathFollowingRequestResult Result;
	FPathFindingQuery Query;
	if (!BuildPathfindingQuery(Request, Query)) return Result;
	FNavPathSharedPtr Path;
	FindPathForMoveRequest(Request, Query, Path);
	if (!Path.IsValid() || !Path->IsValid() || Path->IsPartial()) return Result;

	Result.MoveId = RequestMove(Request, Path);
	if (!Result.MoveId.IsValid()) return Result;

	AnasimusMoveID = Result.MoveId;
	UCharacterMovementComponent* Movement = CastChecked<ACharacter>(GetPawn())->GetCharacterMovement();
	bPreviousAnasimusOrientToMovement = Movement->bOrientRotationToMovement;
	bPreviousAnasimusUseControllerRotation = Movement->bUseControllerDesiredRotation;
	Movement->bOrientRotationToMovement = false;
	Movement->bUseControllerDesiredRotation = true;
	bAllowStrafe = true;
	SetFocus(&Target, AnasimusCombatFocus);
	Result.Code = EPathFollowingRequestResult::RequestSuccessful;
	if (OutPath) *OutPath = Path;
	return Result;
}

void ATBMonsterAIController::StopAnasimusBackstep()
{
	if (IsAnasimusBackstepping()) FinishAnasimusMove(true);
}

void ATBMonsterAIController::FinishAnasimusMove(bool bAbortMove)
{
	if (!AnasimusMoveID.IsValid()) return;

	const FAIRequestID FinishedID = AnasimusMoveID;
	AnasimusMoveID = FAIRequestID::InvalidRequest;
	ClearFocus(AnasimusCombatFocus);
	if (ACharacter* MovingCharacter = GetCharacter())
	{
		UCharacterMovementComponent* Movement = MovingCharacter->GetCharacterMovement();
		Movement->bOrientRotationToMovement = bPreviousAnasimusOrientToMovement;
		Movement->bUseControllerDesiredRotation = bPreviousAnasimusUseControllerRotation;
		if (PreviousBackstepSpeed.IsSet()) Movement->MaxWalkSpeed = PreviousBackstepSpeed.GetValue();
	}
	PreviousBackstepSpeed.Reset();
	if (bAbortMove && GetCurrentMoveRequestID() == FinishedID) StopMovement();
}

void ATBMonsterAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	if (CombatWaitingMoveID.IsValid() && RequestID == CombatWaitingMoveID) FinishCombatWaitingMove(false);
	if (AnasimusMoveID.IsValid() && RequestID == AnasimusMoveID) FinishAnasimusMove(false);
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
	FinishCombatWaitingMove(true);
	bCombatStandOff = false;
	CombatWaitingSlot = INDEX_NONE;
	NextWaitingPlanTime = 0.0;
	FinishAnasimusMove(true);
	if (ACharacter* ControlledCharacter = GetCharacter())
	{
		if (UTBMonsterMovementComponent* Movement = Cast<UTBMonsterMovementComponent>(ControlledCharacter->GetCharacterMovement()))
		{
			Movement->ResetAnasimusEvasion();
		}
	}
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
	const TWeakObjectPtr<AActor> PreviousTarget = TrackedTarget;
	TrackedTarget.Reset();
	if (ATBMonster* Monster = Cast<ATBMonster>(GetPawn()))
	{
		if (UTBCombatTokenSubsystem* Tokens = GetWorld()->GetSubsystem<UTBCombatTokenSubsystem>()) Tokens->ClearTarget(*Monster, PreviousTarget);
	}
}

void ATBMonsterAIController::SetTrackedTarget(AActor* Target)
{
	if (TrackedTarget.Get() == Target) return;
	ClearTrackedTarget();
	TrackedTarget = Target;
	if (IsValid(Target))
	{
		WaitingOrbitDirection = FMath::RandBool() ? 1 : -1;
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
