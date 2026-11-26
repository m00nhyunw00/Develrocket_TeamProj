#pragma once

#include "CoreMinimal.h"
#include "AI/TBBaseAIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "TBCitizenAIController.generated.h"

class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;
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
	
	void GetKnownThreats(TArray<const AActor*>& OutThreats) const;   // EQS Context에 넘길 인지 중인 위협 목록 조회

	void ReportDamage(AActor* DamageSource);      // 피격 시 시민이 호출, 공격자를 위협으로 기억해 즉시 도주

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	
	void ForgetExpiredThreats();                  // 기억 시간이 지난 위협을 목록에서 제거

	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	bool IsThreat(const AActor* Actor) const;
	void RefreshThreatKey(bool bAllowStartle = true);   // 가장 가까운 위협을 블랙보드에 기록, 첫 발견 시 패닉 시작 (false면 놀람 생략)

	UPROPERTY(EditDefaultsOnly, Category = "AI")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;  // BP 컨트롤러에서 BT_Citizen 지정

	UPROPERTY(VisibleAnywhere, Category = "AI|Perception")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Perception")
	float SightRadius = 2000.f;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Perception")
	float LoseSightRadius = 2400.f;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Perception")
	float PeripheralVisionAngle = 70.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "AI|Flee", meta = (ClampMin = "0.0", Units = "s"))
	float ThreatMemorySeconds = 3.f;              // 시야에서 사라진 위협의 기억 유지 시간 (패닉 중 걷기 전환 시점에 영향)

	void StartStartle(AActor* Threat);            // 놀람 몽타주 재생, 시선이 위협을 향하도록 즉시 회전 후 고정
	void FinishStartle();                         // 회전 고정 해제 후 BT 재개
	void EndStartleFacing();                      // 놀람 중 회전 고정 해제, 이동 방향 회전으로 복귀

	UPROPERTY(EditDefaultsOnly, Category = "AI|Flee", meta = (ClampMin = "0.0", Units = "cm"))
	float CalmDistance = 2200.f;                 // 도주 중 걷기로 전환하는 위협 거리

	UPROPERTY(EditDefaultsOnly, Category = "AI|Flee", meta = (ClampMin = "0.0", Units = "cm"))
	float DangerDistance = 1200.f;               // 걷는 중 다시 뛰기로 전환하는 위협 거리

	UPROPERTY(EditDefaultsOnly, Category = "AI|Flee", meta = (ClampMin = "0.0", Units = "s"))
	float StartleCooldown = 6.f;                  // 연속 놀람 방지용 최소 간격

	void UpdatePanic();                           // 패닉 중 위협 거리를 주기적으로 확인해 걷기·뛰기 전환
	void EnterRun(AActor* Threat, bool bAllowStartle = true);   // 뛰기 전환, 거리·쿨타임 조건 충족 시 놀람 재생

	UPROPERTY(EditDefaultsOnly, Category = "AI|Flee", meta = (ClampMin = "0.0", Units = "cm"))
	float NoStartleDistance = 500.f;              // 이 거리 안의 위협은 놀람 없이 바로 도주
	float GetNearestThreat(AActor*& OutThreat) const;   // 인지 중인 위협 중 가장 가까운 거리 반환 (없으면 MAX)

	UPROPERTY(EditDefaultsOnly, Category = "AI|Perception", meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "deg"))
	float PanicVisionAngle = 180.f;               // 도주 중 시야 반각 (최대값이면 전방위)

	UPROPERTY(EditDefaultsOnly, Category = "AI|Perception", meta = (ClampMin = "0.0", Units = "cm"))
	float PanicLoseSightRadius = 3000.f;         // 도주 중 한 번 본 위협을 놓치는 거리

	void SetPanicVision(bool bPanic);             // 평상시·도주 중 시야 설정 전환

	UPROPERTY(VisibleAnywhere, Category = "AI|Perception")
	TObjectPtr<UAISenseConfig_Hearing> HearingConfig;   // 총성처럼 시야 밖 위협 감지용 청각

	UPROPERTY(EditDefaultsOnly, Category = "AI|Perception", meta = (ClampMin = "0.0", Units = "cm"))
	float HearingRange = 4000.f;                  // 총성 감지 거리

	void RememberThreat(AActor* Threat, bool bAllowStartle);   // 보지 못한 위협도 기억해 도주 시작 (피격·총성 공용)

private:
	bool bPanicked = false;                       // 첫 놀람 이후 사망까지 도주 분기 유지
	bool bRunning = false;                        // 도주 중 뛰기·걷기 구분
	double LastStartleTime = -1.0e9;              // 놀람 쿨타임 기준 시각 (초기값은 첫 놀람 즉시 허용)
	FTimerHandle PanicTimer;

	TArray<TWeakObjectPtr<AActor>> VisibleThreats;    // 지금 보이는 위협 목록
	TMap<TWeakObjectPtr<AActor>, double> LostThreatTimes;   // 위협 → 시야에서 사라진 시각
	FTimerHandle ForgetTimer;                               // 주기적으로 만료 검사
	FTimerHandle StartleTimer;
};