#pragma once

#include "UObject/Interface.h"
#include "TBPoolable.generated.h"

UINTERFACE(BlueprintType)
class DEVELROCKET_TEAMPROJ_API UTBPoolable : public UInterface
{
	GENERATED_BODY()
};

class DEVELROCKET_TEAMPROJ_API ITBPoolable
{
	GENERATED_BODY()

public:
	// 풀에서 활성화될 때 호출됩니다. 액터별 게임플레이·이동·AI·시각 상태를 복구합니다.
	virtual void OnPoolActivated();

	// 풀에 반환되기 전에 호출됩니다. 액터별 동작을 중지하고 필요한 상태를 정리합니다.
	virtual void OnPoolDeactivated();
};
