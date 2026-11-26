#include "AI/EQS/TBEQC_CitizenThreats.h"

#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Actor.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AI/AIControler/NPC/TBCitizenAIController.h"

void UTBEQC_CitizenThreats::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	// BT에서 실행한 쿼리라면 Owner는 폰
	const APawn* Querier = Cast<APawn>(QueryInstance.Owner.Get());
	const ATBCitizenAIController* AIC = Querier ? Cast<ATBCitizenAIController>(Querier->GetController()) : nullptr;
	if (!AIC)
	{
		return;
	}
	
	TArray<const AActor*> Threats;
	AIC->GetKnownThreats(Threats);
	
	// 안전장치 : 블랙보드에 남아있는 위협도 포함
	if (const UBlackboardComponent* BB = AIC->GetBlackboardComponent())
	{
		if (const AActor* Remembered = Cast<AActor>(BB->GetValueAsObject(ATBCitizenAIController::ThreatActorKey)))
		{
			Threats.AddUnique(Remembered);
		}
	}
	
	if (Threats.Num() > 0)
	{
		UEnvQueryItemType_Actor::SetContextHelper(ContextData, Threats);
	}
}
