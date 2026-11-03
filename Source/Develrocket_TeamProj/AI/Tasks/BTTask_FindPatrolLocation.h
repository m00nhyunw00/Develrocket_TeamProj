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
	
	//순찰 위취를 찾을 반경 
	UPROPERTY(EditAnywhere, Category="Patrol")
	float SearchRadius = 1000.f;
};