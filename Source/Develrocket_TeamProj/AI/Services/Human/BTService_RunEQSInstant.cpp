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
	bCallTickOnSearchStart = true;                // 분기 진입 즉시 계산해 MoveTo 시작 전에 목적지 기록
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

	FEnvQueryRequest Request(QueryTemplate, Pawn);   // Querier를 시민 폰으로 지정 (Context가 폰에서 컨트롤러 조회)
	const TSharedPtr<FEnvQueryResult> Result = EQSManager->RunInstantQuery(Request, EEnvQueryRunMode::SingleResult);

	if (Result.IsValid() && Result->IsSuccessful() && Result->Items.Num() > 0)
	{
		BB->SetValueAsVector(GetSelectedBlackboardKey(), Result->GetItemAsLocation(0));
	}
	// 모든 Option 실패 시 이전 목적지 유지
}
