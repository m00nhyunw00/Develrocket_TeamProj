#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_FindPatrolLocation.generated.h"

UCLASS()
class DEVELROCKET_TEAMPROJ_API UBTTask_FindPatrolLocation :public UBTTask_BlackboardBase
{
	GENERATED_BODY()
	
public:
	UBTTask_FindPatrolLocation();
	
protected:
	//BT가 이 노드에 도착했을 때 실행되는 본체
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	
	// 순찰 대상으로 검색할 액터 클래스
	UPROPERTY(EditAnywhere, Category="Patrol")
	TSubclassOf<AActor> PatrolPointClass;

	UPROPERTY(EditAnywhere, Category="Patrol", meta=(ClampMin="0.0", Units="cm"))
	float SearchRadius = 8000.f;

	// 같은 프레임에 여러 AI가 경로를 찾지 않도록 각 태스크 실행을 분산한다.
	UPROPERTY(EditAnywhere, Category="Patrol", meta=(ClampMin="0.0", Units="s"))
	float MaxSearchDelay = 0.5f;

private:
	float RemainingSearchDelay = 0.0f;
};
