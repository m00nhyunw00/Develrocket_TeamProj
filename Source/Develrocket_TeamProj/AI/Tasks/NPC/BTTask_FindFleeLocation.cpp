#include "AI/Tasks/NPC/BTTask_FindFleeLocation.h"

#include "AIController.h"                         // OwnerComp.GetAIOwner()
#include "BehaviorTree/BlackboardComponent.h"     // 블랙보드 값 읽기/쓰기
#include "BehaviorTree/BlackboardData.h"          // ResolveSelectedKey에 넘길 블랙보드 에셋 타입
#include "NavigationSystem.h"                     // NavMesh 위 지점 찾기

UBTTask_FindFleeLocation::UBTTask_FindFleeLocation()
{
	NodeName = TEXT("TB Find Flee Location");     // BT 에디터에 보일 노드 이름

	// 결과 키(BlackboardKey)는 Vector 키만 고를 수 있게 제한
	BlackboardKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(ThisClass, BlackboardKey));

	// 입력 키(ThreatKey)는 Actor 오브젝트 키만 고를 수 있게 제한
	ThreatKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(ThisClass, ThreatKey), AActor::StaticClass());
}

void UBTTask_FindFleeLocation::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);            // 부모가 BlackboardKey를 연결

	// 직접 추가한 키 선택자는 직접 연결해야 함 (안 하면 SelectedKeyName이 비어 값을 못 읽음)
	if (UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		ThreatKey.ResolveSelectedKey(*BBAsset);
	}
}

EBTNodeResult::Type UBTTask_FindFleeLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();            // 이 BT를 실행 중인 컨트롤러
	APawn* MyPawn = AIController ? AIController->GetPawn() : nullptr; // 조종 중인 시민
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();

	if (!MyPawn || !Blackboard)                   // 필요한 것이 없으면 실패 → 부모 Sequence 중단
	{
		return EBTNodeResult::Failed;
	}

	// 블랙보드에서 위협 액터 읽기
	const AActor* Threat = Cast<AActor>(Blackboard->GetValueAsObject(ThreatKey.SelectedKeyName));
	if (!Threat)
	{
		return EBTNodeResult::Failed;
	}

	const FVector MyLocation = MyPawn->GetActorLocation();

	FVector AwayDir = MyLocation - Threat->GetActorLocation();   // 위협 → 나 방향 = 도망 방향
	AwayDir.Z = 0.f;                                              // 수평으로만
	AwayDir = AwayDir.GetSafeNormal();                            // 길이 1로

	if (AwayDir.IsNearlyZero())                   // 위치가 겹친 특수한 경우
	{
		AwayDir = MyPawn->GetActorForwardVector();
	}

	const FVector DesiredLocation = MyLocation + AwayDir * FleeDistance;  // 이상적인 도망 지점

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(MyPawn->GetWorld());
	if (!NavSys)
	{
		return EBTNodeResult::Failed;
	}

	FNavLocation NavLocation;                     // 실제 이동 가능한 지점이 담길 변수

	// 1순위: 이상적인 지점을 NavMesh 위로 투영 / 2순위: 내 주변 도달 가능한 아무 지점
	const bool bFound =
		NavSys->ProjectPointToNavigation(DesiredLocation, NavLocation, FVector(ProjectExtent))
		|| NavSys->GetRandomReachablePointInRadius(MyLocation, FleeDistance, NavLocation);
		
		
	UE_LOG(LogTemp, Warning, TEXT("[Citizen] FleeLocation found=%d"), bFound);   // NavMesh 위 지점을 찾았는지

	if (!bFound)
	{
		return EBTNodeResult::Failed;
	}

	Blackboard->SetValueAsVector(BlackboardKey.SelectedKeyName, NavLocation.Location);  // 결과를 FleeLocation에 기록
	return EBTNodeResult::Succeeded;              // 즉시 끝나는 태스크 → 다음 노드(MoveTo)로 진행
}