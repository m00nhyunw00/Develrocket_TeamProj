#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TBGameModeBase.generated.h"

// 폰 클래스를 PlayerState의 직업 데이터에서 결정하는 공통 GameMode
UCLASS()
class DEVELROCKET_TEAMPROJ_API ATBGameModeBase : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
};