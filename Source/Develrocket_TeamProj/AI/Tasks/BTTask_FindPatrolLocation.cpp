#include "AI/Tasks/BTTask_FindPatrolLocation.h"
#include "AI/TBBaseAIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"

UBTTask_FindPatrolLocation::UBTTask_FindPatrolLocation()
{
	NodeName = TEXT("TB Find Patrol Location");
	
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

	const AActor* Point = Controller->FindReachablePatrolPoint(PatrolPointClass, SearchRadius);
	if (!IsValid(Point))
	{
		return EBTNodeResult::Failed;
	}

	Blackboard->SetValueAsVector(BlackboardKey.SelectedKeyName, Point->GetActorLocation());
	return EBTNodeResult::Succeeded;
}
