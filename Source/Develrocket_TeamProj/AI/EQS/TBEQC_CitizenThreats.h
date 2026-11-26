#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "TBEQC_CitizenThreats.generated.h"

// 시민이 알고 있는 위협 액터들을 EQS에 넘겨준다
UCLASS(meta=(DisplayName = "Citizen Threats"))
class DEVELROCKET_TEAMPROJ_API UTBEQC_CitizenThreats : public UEnvQueryContext
{
	GENERATED_BODY()
	
public:
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};
