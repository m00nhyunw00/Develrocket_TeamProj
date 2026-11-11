#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "TBCitizenAIController.generated.h"

class UAISenseConfig_Sight;
class UBehaviorTree;

UCLASS()
class DEVELROCKET_TEAMPROJ_API ATBCitizenAIController : public AAIController
{
	GENERATED_BODY()

public:
	ATBCitizenAIController();

	void StopFleeing();                           // 사망 시 시민이 호출: BT와 감지 정지

	static const FName ThreatActorKey;            // 블랙보드 키 이름을 한 곳에서 관리 (문자열 오타 방지)

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

private:
	TArray<TWeakObjectPtr<AActor>> VisibleThreats;    // 지금 보이는 위협 목록
	TMap<TWeakObjectPtr<AActor>, double> LostThreatTimes;   // 위협 → 시야에서 사라진 시각
	FTimerHandle ForgetTimer;                               // 주기적으로 만료 검사
};