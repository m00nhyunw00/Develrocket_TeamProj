#include "AI/AIControler/NPC/TBCitizenAIController.h"

#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "BehaviorTree/BehaviorTree.h"            // RunBehaviorTree에 넘길 에셋 타입
#include "BehaviorTree/BlackboardComponent.h"     // 블랙보드 값 쓰기
#include "BrainComponent.h"                       // StopLogic
#include "Character/Player/TBPlayer.h"
#include "Character/Monster/TBMonster.h"

const FName ATBCitizenAIController::ThreatActorKey(TEXT("ThreatActor"));   // BB_Citizen의 키 이름과 똑같아야 함

void ATBCitizenAIController::PauseForDrain()
{
	// 실행 중이던 MoveTo 취소
	StopMovement();
	
	// 몬스터 컨트룰러와 같은 조건: 실행 중이고 아직 멈추지 않았을 때
	if (UBrainComponent* Brain = GetBrainComponent(); Brain && Brain->IsRunning() && !Brain->IsPaused())
	{
		Brain->PauseLogic(TEXT("Citizen Drained"));
	}
}

void ATBCitizenAIController::ResumeFromDrain()
{
	if (UBrainComponent* Brain = GetBrainComponent(); Brain && Brain->IsPaused())
	{
		Brain->ResumeLogic(TEXT("Citizen Drain Cancelled"));
	}
}

ATBCitizenAIController::ATBCitizenAIController()
{
	// Perception 컴포넌트를 만들어 AIController에 등록
	UAIPerceptionComponent* PerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComp"));
	SetPerceptionComponent(*PerceptionComp);

	// 시야 설정 객체를 만들고 Perception에 연결
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	PerceptionComp->ConfigureSense(*SightConfig);

	// 여러 감각 중 시야를 대표 감각으로 지정
	PerceptionComp->SetDominantSense(UAISense_Sight::StaticClass());
}

void ATBCitizenAIController::BeginPlay()
{
	Super::BeginPlay();                           // 부모 BeginPlay 먼저 호출 (필수)

	if (!HasAuthority())                          // AI 판단은 서버에서만 (클라는 결과만 복제받음)
	{
		return;
	}

	// 생성자 시점엔 BP에서 바꾼 값이 반영되지 않으므로 BeginPlay에서 수치 적용 (TBMonsterAIController와 같은 방식)
	SightConfig->SightRadius = SightRadius;
	SightConfig->LoseSightRadius = LoseSightRadius;
	SightConfig->PeripheralVisionAngleDegrees = PeripheralVisionAngle;

	// 팀(Affiliation) 구분을 아직 안 쓰므로 적/중립/아군 모두 감지
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

	GetPerceptionComponent()->ConfigureSense(*SightConfig);    // 바뀐 수치를 다시 등록

	// "무언가 보였다/사라졌다" 이벤트에 내 함수를 연결
	GetPerceptionComponent()->OnTargetPerceptionUpdated.AddDynamic(this, &ThisClass::HandleTargetPerceptionUpdated);
}

void ATBCitizenAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (!HasAuthority())
	{
		return;
	}

	// BT 실행: BT 에셋에 지정된 블랙보드(BB_Citizen)도 자동으로 생성·연결됨
	if (BehaviorTreeAsset)
	{
		RunBehaviorTree(BehaviorTreeAsset);
	}

	// 풀링 재사용 대비: 꺼져 있던 시야 다시 켜기
	if (UAIPerceptionComponent* Perception = GetPerceptionComponent())
	{
		Perception->SetSenseEnabled(UAISense_Sight::StaticClass(), true);
	}
}

void ATBCitizenAIController::OnUnPossess()
{
	VisibleThreats.Reset();
	LostThreatTimes.Reset();
	GetWorldTimerManager().ClearTimer(ForgetTimer);
	
	Super::OnUnPossess();	
}

void ATBCitizenAIController::StopFleeing()
{
	VisibleThreats.Reset();

	if (UBlackboardComponent* BB = GetBlackboardComponent())
	{
		BB->ClearValue(ThreatActorKey);           // 위협 정보 지우기
	}

	StopMovement();                               // 진행 중인 MoveTo 취소

	if (UBrainComponent* Brain = GetBrainComponent())
	{
		Brain->StopLogic(TEXT("Citizen Dead"));   // BT 자체를 정지 (죽은 시민이 다시 움직이지 않게)
	}

	if (UAIPerceptionComponent* Perception = GetPerceptionComponent())
	{
		Perception->SetSenseEnabled(UAISense_Sight::StaticClass(), false);
	}
	
	LostThreatTimes.Reset();
	GetWorldTimerManager().ClearTimer(ForgetTimer);    // 정리 코드
}

void ATBCitizenAIController::HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	UE_LOG(LogTemp, Warning, TEXT("[Citizen] Perceived %s / Sensed=%d / Threat=%d"), *GetNameSafe(Actor), Stimulus.WasSuccessfullySensed(), IsThreat(Actor));   // 감지 이벤트가 오는지, 위협으로 판정되는지
	
	if (!IsThreat(Actor))
	{
		return;
	}

	if (Stimulus.WasSuccessfullySensed())         // 다시 보임
	{
		VisibleThreats.AddUnique(Actor);
		LostThreatTimes.Remove(Actor);            // 다시 보였으니 "잃어버린 시각" 기록 삭제
	}
	else                                          // 시야에서 사라짐 → 바로 지우지 않고 시각만 기록
	{
		LostThreatTimes.Add(Actor, GetWorld()->GetTimeSeconds());

		if (!GetWorldTimerManager().IsTimerActive(ForgetTimer))   // 0.5초마다 만료 검사 시작
		{
			GetWorldTimerManager().SetTimer(ForgetTimer, this, &ThisClass::ForgetExpiredThreats, 0.5f, true);
		}
	}

	RefreshThreatKey();
}

bool ATBCitizenAIController::IsThreat(const AActor* Actor) const
{
	if (!IsValid(Actor) || Actor == GetPawn())    // 없는 대상이거나 자기 자신이면 위협 아님
	{
		return false;
	}

	if (Actor->IsA<ATBPlayer>())                  // 플레이어는 항상 위협
	{
		return true;
	}

	if (const ATBMonster* Monster = Cast<ATBMonster>(Actor))
	{
		return !Monster->IsDead();                // 살아 있는 몬스터만 위협
	}

	return false; 
}

void ATBCitizenAIController::RefreshThreatKey()
{
	UBlackboardComponent* BB = GetBlackboardComponent();
	const APawn* MyPawn = GetPawn();
	if (!BB || !MyPawn)
	{
		return;
	}

	// 사라진 위협 정리
	VisibleThreats.RemoveAll([](const TWeakObjectPtr<AActor>& Threat) { return !Threat.IsValid(); });

	// 가장 가까운 위협 찾기
	AActor* Closest = nullptr;
	float ClosestDistSq = TNumericLimits<float>::Max();
	for (const TWeakObjectPtr<AActor>& Threat : VisibleThreats)
	{
		const float DistSq = FVector::DistSquared(MyPawn->GetActorLocation(), Threat->GetActorLocation());  // 제곱 거리: sqrt 생략으로 비교만 할 때 더 빠름
		if (DistSq < ClosestDistSq)
		{
			ClosestDistSq = DistSq;
			Closest = Threat.Get();
		}
	}

	if (Closest)
	{
		BB->SetValueAsObject(ThreatActorKey, Closest);   // 도망 분기 활성화
	}
	else
	{
		BB->ClearValue(ThreatActorKey);                  // 평상시 분기로 복귀
	}
}

void ATBCitizenAIController::ForgetExpiredThreats()
{
	const double Now = GetWorld()->GetTimeSeconds();

	for (auto It = LostThreatTimes.CreateIterator(); It; ++It)        // 순회하면서 안전하게 지우려면 이터레이터 사용
	{
		if (!It.Key().IsValid() || Now - It.Value() >= ThreatMemorySeconds)
		{
			VisibleThreats.Remove(It.Key());      // 기억 시간이 지났으니 위협 목록에서 제거
			It.RemoveCurrent();
		}
	}

	if (LostThreatTimes.IsEmpty())                // 더 검사할 것이 없으면 타이머 정지
	{
		GetWorldTimerManager().ClearTimer(ForgetTimer);
	}

	RefreshThreatKey();                           // 남은 위협 기준으로 블랙보드 갱신 (없으면 ClearValue)
}

void ATBCitizenAIController::ReactivateFromPool()
{
	if (!HasAuthority())
	{
		return;
	}

	// 이전 생에서 기억하던 위협 정리
	VisibleThreats.Reset();
	LostThreatTimes.Reset();
	GetWorldTimerManager().ClearTimer(ForgetTimer);

	if (UBlackboardComponent* BB = GetBlackboardComponent())
	{
		BB->ClearValue(ThreatActorKey);
	}

	// StopFleeing에서 꺼둔 시야를 다시 켜고, 이미 보고 있던 대상도 새로 감지하도록 갱신
	if (UAIPerceptionComponent* Perception = GetPerceptionComponent())
	{
		Perception->ForgetAll();
		Perception->SetSenseEnabled(UAISense_Sight::StaticClass(), true);
		Perception->RequestStimuliListenerUpdate();
	}
	
	// bIsPaused 해제 + 블랙보드 알림 재개
	if (UBrainComponent* Brain = GetBrainComponent())
	{
		if (Brain->IsPaused())
		{
			Brain->ResumeLogic(TEXT("Citizen Reactivated"));   
		}
		Brain->RestartLogic();
	}
}