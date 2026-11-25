#include "AI/Services/Human/BTService_UpdateFleeLocation.h"

#include "AIController.h"                         // OwnerComp.GetAIOwner()
#include "BehaviorTree/BlackboardComponent.h"     // 블랙보드 값 읽기/쓰기
#include "BehaviorTree/BlackboardData.h"          // ResolveSelectedKey에 넘길 블랙보드 에셋 타입
#include "NavigationSystem.h"                     // NavMesh 위 지점 찾기

UBTService_UpdateFleeLocation::UBTService_UpdateFleeLocation()
{
	NodeName = TEXT("TB Update Flee Location");   // BT 에디터에 보일 노드 이름

	Interval = 0.4f;                              // 450cm/s 기준 약 180cm마다 목적지를 앞으로 이동 → 800cm 지점에 도착하기 전에 항상 갱신
	RandomDeviation = 0.1f;                       // 여러 시민이 같은 프레임에 몰려서 계산하지 않도록 간격을 살짝 흩뜨림
	bCallTickOnSearchStart = true;                // 분기 진입 즉시 1회 계산 → MoveTo가 시작되기 전에 FleeLocation이 채워짐
	bNotifyCeaseRelevant = true;                  // 분기를 벗어날 때 OnCeaseRelevant 호출 받기

	// 결과 키(BlackboardKey)는 Vector 키만 고를 수 있게 제한
	BlackboardKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(ThisClass, BlackboardKey));

	// 입력 키(ThreatKey)는 Actor 오브젝트 키만 고를 수 있게 제한
	ThreatKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(ThisClass, ThreatKey), AActor::StaticClass());
}

void UBTService_UpdateFleeLocation::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);            // 부모가 BlackboardKey를 연결

	// 직접 추가한 키 선택자는 직접 연결해야 함 (안 하면 SelectedKeyName이 비어 값을 못 읽음)
	if (UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		ThreatKey.ResolveSelectedKey(*BBAsset);
	}
}

void UBTService_UpdateFleeLocation::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	AAIController* AIController = OwnerComp.GetAIOwner();            // 이 BT를 실행 중인 컨트롤러
	APawn* MyPawn = AIController ? AIController->GetPawn() : nullptr; // 조종 중인 시민
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!MyPawn || !Blackboard)
	{
		return;
	}

	// 블랙보드에서 위협 액터 읽기 (없으면 이번 틱은 목적지 유지)
	const AActor* Threat = Cast<AActor>(Blackboard->GetValueAsObject(ThreatKey.SelectedKeyName));
	if (!Threat)
	{
		return;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(MyPawn->GetWorld());
	if (!NavSys)
	{
		return;
	}

	const FVector MyLocation = MyPawn->GetActorLocation();

	FVector AwayDir = MyLocation - Threat->GetActorLocation();   // 위협 → 나 방향 = 도망 방향
	AwayDir.Z = 0.f;                                              // 수평으로만
	AwayDir = AwayDir.GetSafeNormal();                            // 길이 1로

	if (AwayDir.IsNearlyZero())                   // 위치가 겹친 특수한 경우
	{
		AwayDir = MyPawn->GetActorForwardVector();
	}

	FNavLocation NavLocation;                     // 실제 이동 가능한 지점이 담길 변수

	// 1순위: 위협 반대 방향의 이상적인 지점을 NavMesh 위로 투영
	if (NavSys->ProjectPointToNavigation(MyLocation + AwayDir * FleeDistance, NavLocation, FVector(ProjectExtent)))
	{
		Blackboard->SetValueAsVector(BlackboardKey.SelectedKeyName, NavLocation.Location);
		return;
	}

	// 2순위(투영 실패, 예: 벽 앞): 매 틱 랜덤을 고르면 0.4초마다 방향이 바뀌어 지그재그가 됨
	// → 목적지가 아직 없거나 거의 도착했을 때만 랜덤 지점으로 교체, 그 외엔 기존 목적지 유지
	const bool bHasTarget = Blackboard->IsVectorValueSet(BlackboardKey.SelectedKeyName);
	const FVector CurrentTarget = Blackboard->GetValueAsVector(BlackboardKey.SelectedKeyName);
	const bool bNeedNewTarget = !bHasTarget || FVector::Dist2D(MyLocation, CurrentTarget) < RandomFallbackDistance;

	if (bNeedNewTarget && NavSys->GetRandomReachablePointInRadius(MyLocation, FleeDistance, NavLocation))
	{
		Blackboard->SetValueAsVector(BlackboardKey.SelectedKeyName, NavLocation.Location);
	}
}

void UBTService_UpdateFleeLocation::OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// 도망 분기를 벗어나면 목적지 비우기 → 다음 도망 때 지난번 목적지로 먼저 뛰어가지 않음
	if (UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent())
	{
		Blackboard->ClearValue(BlackboardKey.SelectedKeyName);
	}

	Super::OnCeaseRelevant(OwnerComp, NodeMemory);
}