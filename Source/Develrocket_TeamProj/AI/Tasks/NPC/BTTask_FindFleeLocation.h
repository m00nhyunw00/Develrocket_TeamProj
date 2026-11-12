#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"   // 블랙보드 키 하나(BlackboardKey)를 기본 제공하는 태스크 부모
#include "BTTask_FindFleeLocation.generated.h"

UCLASS()
class DEVELROCKET_TEAMPROJ_API UBTTask_FindFleeLocation : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTTask_FindFleeLocation();                   // 노드 이름, 키 필터 설정

protected:
	// BT 에셋이 로드될 때 호출: 추가로 만든 키 선택자(ThreatKey)를 실제 블랙보드 키와 연결
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

	// BT가 이 노드에 도착했을 때 실행되는 본체
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	// 입력 키: 도망칠 대상 (부모의 BlackboardKey는 결과 키 FleeLocation으로 사용)
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector ThreatKey;

	UPROPERTY(EditAnywhere, Category = "Flee", meta = (Units = "cm"))
	float FleeDistance = 800.f;                   // 한 번에 도망갈 거리

	UPROPERTY(EditAnywhere, Category = "Flee", meta = (Units = "cm"))
	float ProjectExtent = 300.f;                  // 목표 지점 주변에서 NavMesh를 찾을 범위
};