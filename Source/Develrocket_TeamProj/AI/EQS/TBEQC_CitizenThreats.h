#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "TBEQC_CitizenThreats.generated.h"

// 시민이 인지 중인 위협 액터를 EQS Test 기준점으로 제공
UCLASS(meta=(DisplayName = "Citizen Threats"))
class DEVELROCKET_TEAMPROJ_API UTBEQC_CitizenThreats : public UEnvQueryContext
{
	GENERATED_BODY()
	
public:
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};
