#include "AI/Tasks/BTTask_FindPatrolLocation.h"
#include "AI/TBBaseAIController.h"
#include "AI/TBPatrolPoint.h"
#include "Character/Monster/TBMonster.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"

UBTTask_FindPatrolLocation::UBTTask_FindPatrolLocation()
{
	NodeName = TEXT("TB Find Patrol Location");
	bCreateNodeInstance = true;
	bNotifyTick = true;
	
	BlackboardKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(ThisClass, BlackboardKey));
}

EBTNodeResult::Type UBTTask_FindPatrolLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	ATBBaseAIController* Controller = Cast<ATBBaseAIController>(OwnerComp.GetAIOwner());
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!IsValid(Controller) || !IsValid(Blackboard) || !PatrolPointClass || !FMath::IsFinite(SearchRadius) || SearchRadius <= 0.0f
		|| BlackboardKey.GetSelectedKeyID() == FBlackboard::InvalidKey
		|| Blackboard->GetKeyType(BlackboardKey.GetSelectedKeyID()) != UBlackboardKeyType_Vector::StaticClass())
	{
		return EBTNodeResult::Failed;
	}
	RemainingSearchDelay = FMath::FRandRange(0.0f, FMath::Max(0.0f, MaxSearchDelay));
	ScanDuration = 0.0f;
	PreferredLegLength = 0.0f;
	if (const ATBMonster* Monster = Cast<ATBMonster>(Controller->GetPawn()))
	{
		const ATBPatrolPoint* Point = Controller->GetReachedPatrolPoint();
		const bool bNewPoint = IsValid(Point) && Point != LastScannedPoint.Get()
			&& FVector::DistSquared2D(Point->GetActorLocation(), Monster->GetActorLocation()) <= FMath::Square(300.0);
		switch (Monster->GetMonsterBehavior())
		{
		case ETBMonsterBehavior::Boris:
			if (bNewPoint && Point->IsJunction()) ScanDuration = 0.85f;
			break;
		case ETBMonsterBehavior::Demon:
			PreferredLegLength = 950.0f;
			break;
		case ETBMonsterBehavior::Anasimus:
			PreferredLegLength = 500.0f;
			break;
		case ETBMonsterBehavior::SpideEye:
			PreferredLegLength = 300.0f;
			if (bNewPoint) ScanDuration = 0.35f;
			break;
		case ETBMonsterBehavior::Hunter:
			PreferredLegLength = 1100.0f;
			if (bNewPoint && Point->IsJunction()) ScanDuration = 1.1f;
			break;
		default:
			break;
		}
		LastScannedPoint = Point;
		ScanStartYaw = Monster->GetActorRotation().Yaw;
		RemainingSearchDelay += ScanDuration;
	}
	return EBTNodeResult::InProgress;
}

void UBTTask_FindPatrolLocation::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	RemainingSearchDelay -= DeltaSeconds;
	ATBBaseAIController* Controller = Cast<ATBBaseAIController>(OwnerComp.GetAIOwner());
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!IsValid(Controller) || !IsValid(Blackboard))
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}
	if (RemainingSearchDelay > 0.0f)
	{
		if (ATBMonster* Monster = Cast<ATBMonster>(Controller->GetPawn());
			Monster && ScanDuration > 0.0f && !Monster->IsMovementBlocked() && !Blackboard->GetValueAsObject(TEXT("TargetActor")))
		{
			const double Phase = 1.0 - FMath::Clamp(RemainingSearchDelay / ScanDuration, 0.0f, 1.0f);
			FRotator Rotation = Monster->GetActorRotation();
			Rotation.Yaw = ScanStartYaw + 32.0 * FMath::Sin(Phase * 2.0 * UE_DOUBLE_PI);
			Monster->SetActorRotation(Rotation);
		}
		return;
	}
	const AActor* Point = Controller->SelectNextPatrolPoint(PatrolPointClass, SearchRadius, PreferredLegLength);
	if (!IsValid(Point))
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	Blackboard->SetValueAsVector(BlackboardKey.SelectedKeyName, Point->GetActorLocation());
	FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
}
