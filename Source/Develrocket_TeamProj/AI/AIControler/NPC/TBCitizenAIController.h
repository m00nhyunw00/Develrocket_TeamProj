#pragma once

#include "CoreMinimal.h"
#include "AI/TBBaseAIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "TBCitizenAIController.generated.h"

class UAISenseConfig_Sight;
class UBehaviorTree;

UCLASS()
class DEVELROCKET_TEAMPROJ_API ATBCitizenAIController : public ATBBaseAIController
{
	GENERATED_BODY()

public:
	ATBCitizenAIController();

	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;

	void StopFleeing();                           // 사망 시 시민이 호출: BT와 감지 정지

	static const FName ThreatActorKey;            // 블랙보드 키 이름을 한 곳에서 관리 (문자열 오타 방지)
	
	void PauseForDrain();                        // 흡혈당하는 동안 BT 일시 정지
	void ResumeFromDrain();                      // 흡혈이 취소 되면 BT 재개
	void ReactivateFromPool();                     // 풀에서 다시 꺼낼 떄 감지, BT 초기화
	
	void GetKnownThreats(TArray<const AActor*>& OutThreats) const;   // EQS Context용: 지금 알고 있는 위협 목록

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	
	void ForgetExpiredThreats();                  // 기억 시간이 지난 위협을 목록에서 제거

	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	bool IsThreat(const AActor* Actor) const;
	void RefreshThreatKey();                      // 보이는 위협 중 가장 가까운 것을 블랙보드에 기록

	UPROPERTY(EditDefaultsOnly, Category = "AI")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;  // BP 컨트롤러에서 BT_Citizen 지정

	UPROPERTY(VisibleAnywhere, Category = "AI|Perception")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Perception")
	float SightRadius = 1200.f;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Perception")
	float LoseSightRadius = 1500.f;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Perception")
	float PeripheralVisionAngle = 70.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "AI|Flee", meta = (ClampMin = "0.0", Units = "s"))
	float ThreatMemorySeconds = 3.f;              // 시야에서 사라진 뒤에도 이 시간 동안은 계속 도망 
	
	void StartStartle(AActor* Threat);            // 놀람 몽타주 재생 + 시선이 위협을 향하도록 몸 회전
	void FinishStartle();
	void EndStartleFacing();                      // 놀람 회전 해제 (이동 방향 바라보기로 복귀)

	UPROPERTY(EditDefaultsOnly, Category = "AI|Flee", meta = (ClampMin = "0.0", Units = "cm"))
	float CalmDistance = 1400.f;                  // 뛰는 중: 가장 가까운 위협이 이보다 멀어지면 걷기

	UPROPERTY(EditDefaultsOnly, Category = "AI|Flee", meta = (ClampMin = "0.0", Units = "cm"))
	float DangerDistance = 1000.f;                // 걷는 중: 위협이 이보다 가까워지면 다시 뛰기(+놀람)

	UPROPERTY(EditDefaultsOnly, Category = "AI|Flee", meta = (ClampMin = "0.0", Units = "s"))
	float StartleCooldown = 6.f;                  // 다시 놀라기까지 최소 간격

	void UpdatePanic();                           // 0.3초마다: 거리로 걷기/뛰기/재놀람 결정
	void EnterRun(AActor* Threat);                // 뛰기 시작 (쿨타임 지났으면 놀람)
	float GetNearestThreat(AActor*& OutThreat) const;   // 알고 있는 위협 중 가장 가까운 것까지 거리 (없으면 MAX)

private:
	bool bPanicked = false;                       // 한 번 놀랐으면 죽을 때까지 도망 분기 유지
	bool bRunning = false;                        // 지금 뛰는 중인지
	double LastStartleTime = -1.0e9;              // 마지막으로 놀란 시각 (처음엔 아주 옛날)
	FTimerHandle PanicTimer;

	TArray<TWeakObjectPtr<AActor>> VisibleThreats;    // 지금 보이는 위협 목록
	TMap<TWeakObjectPtr<AActor>, double> LostThreatTimes;   // 위협 → 시야에서 사라진 시각
	FTimerHandle ForgetTimer;                               // 주기적으로 만료 검사
	FTimerHandle StartTimer;
	FTimerHandle StartleTimer;
};