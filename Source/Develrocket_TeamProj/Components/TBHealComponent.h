#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TBHealComponent.generated.h"

class UGameplayEffect;

// 회복의 단일 창구: 수치는 갖지 않고, 회복 GE를 만들어 자기 ASC에 적용만 함
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class DEVELROCKET_TEAMPROJ_API UTBHealComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTBHealComponent();

	// [서버] Amount만큼 Blood 회복. 적용 성공 여부 반환
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Heal")
	bool ApplyHeal(float Amount);

protected:
	// Instant, Blood Add, SetByCaller(Data.Heal)인 GE
	UPROPERTY(EditDefaultsOnly, Category = "Heal")
	TSubclassOf<UGameplayEffect> HealEffectClass;
};