#include "AI/AIControler/NPC/TBCitizenAIController.h"

#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISense_Hearing.h"
#include "BehaviorTree/BehaviorTree.h"            // RunBehaviorTree에 넘길 에셋 타입
#include "BehaviorTree/BlackboardComponent.h"     // 블랙보드 값 쓰기
#include "BrainComponent.h"                       // StopLogic
#include "Character/Player/TBPlayer.h"
#include "Character/Monster/TBMonster.h"
#include "Character/Human/TBCitizen.h"

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

	GetWorldTimerManager().ClearTimer(StartleTimer);
	EndStartleFacing();
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

	// 총성처럼 시야 밖 위협 감지용 청각 등록 (보는 방향과 무관)
	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
	PerceptionComp->ConfigureSense(*HearingConfig);

	// 여러 감각 중 시야를 대표 감각으로 지정
	PerceptionComp->SetDominantSense(UAISense_Sight::StaticClass());
}

ETeamAttitude::Type ATBCitizenAIController::GetTeamAttitudeTowards(const AActor& Other) const
{
	if (Other.IsA<ATBPlayer>() || Other.IsA<ATBMonster>()) return ETeamAttitude::Hostile;
	if (Other.IsA<ATBCitizen>()) return ETeamAttitude::Friendly;
	return ETeamAttitude::Neutral;
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

	// Detect only actors classified as threats by GetTeamAttitudeTowards.
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = false;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = false;

	GetPerceptionComponent()->ConfigureSense(*SightConfig);    // 바뀐 수치를 다시 등록

	HearingConfig->HearingRange = HearingRange;
	// 청각은 팀 ID로만 피아를 구분해 GetTeamAttitudeTowards 미사용 (팀 ID 미설정 시 모두 Friendly 판정)
	// 모든 소리를 받고 위협 여부는 HandleTargetPerceptionUpdated의 IsThreat에서 판단
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
	GetPerceptionComponent()->ConfigureSense(*HearingConfig);

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
	GetWorldTimerManager().ClearTimer(StartleTimer);
	EndStartleFacing();                           // 폰 참조가 남아 있는 Super 호출 전에 회전 고정 해제

	// 패닉 상태와 도주 시야 초기화
	bPanicked = false;
	bRunning = false;
	GetWorldTimerManager().ClearTimer(PanicTimer);
	SetPanicVision(false);

	Super::OnUnPossess();
}

void ATBCitizenAIController::StopFleeing()
{
	VisibleThreats.Reset();

	if (UBlackboardComponent* BB = GetBlackboardComponent())
	{
		BB->ClearValue(ThreatActorKey);           // 위협 정보 지우기
	}
	
	if (ATBCitizen* Citizen = Cast<ATBCitizen>(GetPawn()))
	{
		Citizen->SetFleeing(false);
	}

	StopMovement();                               // 진행 중인 MoveTo 취소

	if (UBrainComponent* Brain = GetBrainComponent())
	{
		Brain->StopLogic(TEXT("Citizen Dead"));   // BT 자체를 정지 (죽은 시민이 다시 움직이지 않게)
	}

	SetPanicVision(false);                        // 시야 비활성화보다 먼저 평상시 시야 복구 (재설정이 비활성화를 덮어쓰지 않도록 순서 유지)

	if (UAIPerceptionComponent* Perception = GetPerceptionComponent())
	{
		Perception->SetSenseEnabled(UAISense_Sight::StaticClass(), false);
	}
	
	LostThreatTimes.Reset();
	GetWorldTimerManager().ClearTimer(ForgetTimer);    // 정리 코드
	GetWorldTimerManager().ClearTimer(StartleTimer);

	// 놀람 회전과 패닉 상태 초기화
	EndStartleFacing();
	bPanicked = false;
	bRunning = false;
	GetWorldTimerManager().ClearTimer(PanicTimer);
}

void ATBCitizenAIController::HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	UE_LOG(LogTemp, Warning, TEXT("[Citizen] Perceived %s / Sensed=%d / Threat=%d"), *GetNameSafe(Actor), Stimulus.WasSuccessfullySensed(), IsThreat(Actor));   // 감지 이벤트가 오는지, 위협으로 판정되는지
	
	if (!IsThreat(Actor))
	{
		return;
	}

	// 청각 자극은 시야 기록과 분리해 처리
	if (Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>())
	{
		if (Stimulus.WasSuccessfullySensed())
		{
			RememberThreat(Actor, true);          // 총성: 사수 쪽으로 돌아보는 놀람 후 도주
		}
		return;                                   // 청각 만료 이벤트 무시 (시야 놓침 처리로 오인 방지)
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

void ATBCitizenAIController::RefreshThreatKey(bool bAllowStartle)
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
	else if (!bPanicked)
	{
		BB->ClearValue(ThreatActorKey);                  // 패닉 전에만 평상시 분기로 복귀 (패닉 후에는 마지막 위협 유지)
	}

	// 첫 위협 발견 시 패닉 시작: 도주 시야 확대, 뛰기, 주기적 거리 확인
	if (Closest && !bPanicked)
	{
		bPanicked = true;
		SetPanicVision(true);
		EnterRun(Closest, bAllowStartle);
		GetWorldTimerManager().SetTimer(PanicTimer, this, &ThisClass::UpdatePanic, 0.3f, true);
	}
}

void ATBCitizenAIController::SetPanicVision(bool bPanic)
{
	UAIPerceptionComponent* Perception = GetPerceptionComponent();
	if (!SightConfig || !Perception) return;

	SightConfig->PeripheralVisionAngleDegrees = bPanic ? PanicVisionAngle : PeripheralVisionAngle;
	SightConfig->LoseSightRadius = bPanic ? PanicLoseSightRadius : LoseSightRadius;
	Perception->ConfigureSense(*SightConfig);    // 변경한 시야 설정 재등록
}

void ATBCitizenAIController::RememberThreat(AActor* Threat, bool bAllowStartle)
{
	if (!HasAuthority() || !IsThreat(Threat)) return;   // 위협이 아닌 원인 제외

	// 목록에 있고 놓친 시각 기록이 없으면 현재 시야 안의 위협
	const bool bCurrentlySeen = VisibleThreats.Contains(Threat) && !LostThreatTimes.Contains(Threat);
	if (!bCurrentlySeen)
	{
		// 보지 못한 위협은 놓친 상태로 등록해 기억 시간 동안 유지 (이후 시야에 들어오면 Perception이 기록 삭제)
		VisibleThreats.AddUnique(Threat);
		LostThreatTimes.Add(Threat, GetWorld()->GetTimeSeconds());
		if (!GetWorldTimerManager().IsTimerActive(ForgetTimer))
		{
			GetWorldTimerManager().SetTimer(ForgetTimer, this, &ThisClass::ForgetExpiredThreats, 0.5f, true);
		}
	}

	RefreshThreatKey(bAllowStartle);              // 패닉 전이면 여기서 패닉 시작

	if (bPanicked && !bRunning)
	{
		EnterRun(Threat, bAllowStartle);          // 패닉 중 걷던 상태면 다시 뛰기
	}
}

void ATBCitizenAIController::ReportDamage(AActor* DamageSource)
{
	RememberThreat(DamageSource, false);          // 피격 시 놀람 없이 즉시 도주
}

float ATBCitizenAIController::GetNearestThreat(AActor*& OutThreat) const
{
	OutThreat = nullptr;
	const APawn* MyPawn = GetPawn();
	if (!MyPawn) return TNumericLimits<float>::Max();

	float BestDistSq = TNumericLimits<float>::Max();
	for (const TWeakObjectPtr<AActor>& Threat : VisibleThreats)
	{
		AActor* Actor = Threat.Get();
		if (!Actor || !IsThreat(Actor)) continue;

		const float DistSq = FVector::DistSquared2D(MyPawn->GetActorLocation(), Actor->GetActorLocation());   // 높이 차이를 제외한 수평 거리 비교
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			OutThreat = Actor;
		}
	}
	return OutThreat ? FMath::Sqrt(BestDistSq) : TNumericLimits<float>::Max();
}

void ATBCitizenAIController::EnterRun(AActor* Threat, bool bAllowStartle)
{
	ATBCitizen* Citizen = Cast<ATBCitizen>(GetPawn());
	if (!Citizen) return;

	bRunning = true;
	Citizen->SetFleeing(true);

	if (Threat)
	{
		if (UBlackboardComponent* BB = GetBlackboardComponent())
		{
			BB->SetValueAsObject(ThreatActorKey, Threat);   // 도주 기준 위협 갱신
		}

		const float Dist = FVector::Dist2D(Citizen->GetActorLocation(), Threat->GetActorLocation());
		const bool bTooClose = Dist < NoStartleDistance;                    // 근접 위협은 놀람 없이 도주
		const bool bCooledDown = GetWorld()->GetTimeSeconds() - LastStartleTime >= StartleCooldown;

		if (bAllowStartle && !bTooClose && bCooledDown)
		{
			StartStartle(Threat);                 // 원거리 발견이고 쿨타임이 지난 경우만 놀람
		}
	}
}

void ATBCitizenAIController::UpdatePanic()
{
	ATBCitizen* Citizen = Cast<ATBCitizen>(GetPawn());
	if (!Citizen) return;

	AActor* Nearest = nullptr;
	const float Dist = GetNearestThreat(Nearest);

	// 걷기·뛰기 전환 거리를 따로 두어 경계 거리 근처의 잦은 전환 방지
	if (bRunning && Dist > CalmDistance)
	{
		bRunning = false;
		Citizen->SetFleeing(false);               // 충분히 멀어지면 걷기 전환 (도주 방향은 EQS가 계속 결정)
	}
	else if (!bRunning && Dist < DangerDistance)
	{
		EnterRun(Nearest);                        // 걷는 중 위협 접근 시 다시 뛰기 (조건 충족 시 놀람)
	}
}

void ATBCitizenAIController::StartStartle(AActor* Threat)
{
	ATBCitizen* Citizen = Cast<ATBCitizen>(GetPawn());
	UBrainComponent* Brain = GetBrainComponent();
	if (!Citizen || !Threat || !Brain || !Brain->IsRunning() || Brain->IsPaused()) return;

	float LookYawOffset = 0.f;
	const float Duration = Citizen->PlayStartle(LookYawOffset);
	if (Duration <= 0.f)
	{
		return;
	}

	LastStartleTime = GetWorld()->GetTimeSeconds();   // 놀람 쿨타임 기준 시각 기록

	StopMovement();

	// 애니 시선이 정면에서 틀어진 만큼 몸을 반대로 돌려 시선을 위협 방향에 맞춤
	const FVector MyLocation = Citizen->GetActorLocation();
	const FVector ToThreat = (Threat->GetActorLocation() - MyLocation).GetSafeNormal2D();
	const FVector BodyDir = ToThreat.RotateAngleAxis(-LookYawOffset, FVector::UpVector);

	Citizen->SetActorRotation(FRotator(0.f, BodyDir.Rotation().Yaw, 0.f));   // 놀람 시작 시 즉시 회전
	Citizen->SetRotationLocked(true);                                         // 재생 중 회전 고정

	Brain->PauseLogic(TEXT("Citizen Startled"));
	GetWorldTimerManager().SetTimer(StartleTimer, this, &ThisClass::FinishStartle, Duration, false);
}

void ATBCitizenAIController::EndStartleFacing()
{
	if (ATBCitizen* Citizen = Cast<ATBCitizen>(GetPawn()))
	{
		Citizen->SetRotationLocked(false);        // 이동 방향 회전으로 복귀
	}
}

void ATBCitizenAIController::FinishStartle()
{
	EndStartleFacing();                           // 아래 조기 반환과 관계없이 회전 고정 해제

	const ATBCitizen* Citizen = Cast<ATBCitizen>(GetPawn());
	
	// 흡혈 등으로 이미 정지 중이면 생략
	if (!Citizen||Citizen->GetCitizenState() != ETBCitizenState::Idle)
	{
		return;
	}
	
	if (UBrainComponent* Brain = GetBrainComponent(); Brain && Brain->IsPaused())
	{
		Brain->ResumeLogic(TEXT("Citizen Startle Finished"));
	}

}

void ATBCitizenAIController::GetKnownThreats(TArray<const AActor*>& OutThreats) const
{
	OutThreats.Reset();
	for (const TWeakObjectPtr<AActor>& Threat : VisibleThreats)
	{
		const AActor* Actor = Threat.Get();
		if (Actor && IsThreat(Actor))   // 죽었거나 위협이 아니게 된 대상은 제외
		{
			OutThreats.Add(Actor);
		}
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
	GetWorldTimerManager().ClearTimer(StartleTimer);

	// 놀람·패닉 상태와 도주 시야 초기화
	EndStartleFacing();
	bPanicked = false;
	bRunning = false;
	LastStartleTime = -1.0e9;                     // 재사용 시 놀람 쿨타임 초기화
	GetWorldTimerManager().ClearTimer(PanicTimer);
	SetPanicVision(false);

	if (UBlackboardComponent* BB = GetBlackboardComponent())
	{
		BB->ClearValue(ThreatActorKey);
	}
	
	if (ATBCitizen* Citizen = Cast<ATBCitizen>(GetPawn()))
	{
		Citizen->SetFleeing(false);
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