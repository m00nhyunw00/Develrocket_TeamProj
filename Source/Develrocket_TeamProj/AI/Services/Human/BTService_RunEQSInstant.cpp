#include "AI/Services/Human/BTService_RunEQSInstant.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryManager.h"

UBTService_RunEQSInstant::UBTService_RunEQSInstant()
{
	NodeName = TEXT("TB Run EQS Instant");
	Interval = 0.4f;
	RandomDeviation = 0.1f;
	bCallTickOnSearchStart = true;                // 분기 진입 즉시 계산 → MoveTo 전에 FleeLocation 채움
	BlackboardKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(ThisClass, BlackboardKey));
}

void UBTService_RunEQSInstant::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	const AAIController* AIC = OwnerComp.GetAIOwner();
	APawn* Pawn = AIC ? AIC->GetPawn() : nullptr;
	UEnvQueryManager* EQSManager = UEnvQueryManager::GetCurrent(OwnerComp.GetWorld());
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!Pawn || !QueryTemplate || !EQSManager || !BB)
	{
		return;
	}

	FEnvQueryRequest Request(QueryTemplate, Pawn);   // Querier = 시민 폰 (Context가 Owner로 컨트롤러를 찾음)
	const TSharedPtr<FEnvQueryResult> Result = EQSManager->RunInstantQuery(Request, EEnvQueryRunMode::SingleResult);

	if (Result.IsValid() && Result->IsSuccessful() && Result->Items.Num() > 0)
	{
		BB->SetValueAsVector(GetSelectedBlackboardKey(), Result->GetItemAsLocation(0));
	}
	// 실패하면 이전 값 유지 (Option 2까지 모두 실패한 경우)
}
