#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Services/BTService_BlackboardBase.h"
#include "BTService_RunEQSInstant.generated.h"

class UEnvQuery;

// EQS를 동기 실행해 결과 위치를 블랙보드에 기록 (분기 진입 시와 주기적으로)
// 엔진 Run EQS는 비동기라 노드 종료 시 쿼리가 중단되어, 목적지 도착 후 갱신이 끊기는 문제 대응
UCLASS(meta = (DisplayName = "TB Run EQS Instant"))
class DEVELROCKET_TEAMPROJ_API UBTService_RunEQSInstant : public UBTService_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTService_RunEQSInstant();

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "EQS")
	TObjectPtr<UEnvQuery> QueryTemplate;
};
