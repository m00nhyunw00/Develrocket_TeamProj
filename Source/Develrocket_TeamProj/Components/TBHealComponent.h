#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TBHealComponent.generated.h"

// 회복 요청 창구, 회복 GE는 갖지 않고, 실제 적용은 PlayerState에 위임한다.
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DEVELROCKET_TEAMPROJ_API UTBHealComponent : public UActorComponent
{
	GENERATED_BODY()
	
public:
	UTBHealComponent();
	
	// 플레이어의 Blood 회복을 요청, 요청 성공 여부 반환
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Heal")
	bool ApplyHeal();
};