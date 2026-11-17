#include "AI/Tasks/BTTask_FindPatrolLocation.h"
#include "AI/TBBaseAIController.h"
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
	return EBTNodeResult::InProgress;
}

void UBTTask_FindPatrolLocation::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	RemainingSearchDelay -= DeltaSeconds;
	if (RemainingSearchDelay > 0.0f) return;
	ATBBaseAIController* Controller = Cast<ATBBaseAIController>(OwnerComp.GetAIOwner());
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!IsValid(Controller) || !IsValid(Blackboard))
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}
	const AActor* Point = Controller->SelectNextPatrolPoint(PatrolPointClass, SearchRadius);
	if (!IsValid(Point))
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	Blackboard->SetValueAsVector(BlackboardKey.SelectedKeyName, Point->GetActorLocation());
	FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
}
