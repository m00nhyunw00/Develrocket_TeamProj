#include "AI/EQS/TBEQC_CitizenThreats.h"

#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Actor.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AI/AIControler/NPC/TBCitizenAIController.h"

void UTBEQC_CitizenThreats::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	// BT 서비스에서 실행한 쿼리의 Owner는 시민 폰
	const APawn* Querier = Cast<APawn>(QueryInstance.Owner.Get());
	const ATBCitizenAIController* AIC = Querier ? Cast<ATBCitizenAIController>(Querier->GetController()) : nullptr;
	if (!AIC)
	{
		return;
	}
	
	TArray<const AActor*> Threats;
	AIC->GetKnownThreats(Threats);
	
	// 패닉 후 블랙보드에 남은 마지막 위협도 포함 (시야에서 잊은 뒤에도 도주 기준 유지)
	if (const UBlackboardComponent* BB = AIC->GetBlackboardComponent())
	{
		if (const AActor* Remembered = Cast<AActor>(BB->GetValueAsObject(ATBCitizenAIController::ThreatActorKey)))
		{
			Threats.AddUnique(Remembered);
		}
	}
	
	// 위협 전체를 전달해 Test가 후보마다 위협별로 계산
	if (Threats.Num() > 0)
	{
		UEnvQueryItemType_Actor::SetContextHelper(ContextData, Threats);
	}
}
