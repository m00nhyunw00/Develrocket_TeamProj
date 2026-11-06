#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "TBHitInterface.generated.h"

UINTERFACE(BlueprintType)
class DEVELROCKET_TEAMPROJ_API UTBHitInterface : public UInterface
{
	GENERATED_BODY()
};

class DEVELROCKET_TEAMPROJ_API ITBHitInterface
{
	GENERATED_BODY()

public:
	// 서버에서 호출하며, 반환값은 대상이 적중을 받아들였는지 나타냄
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat|Hit")
	bool ReceiveAttackHit(const FGameplayTag& DamageType, float Damage, AActor* DamageSource, const FHitResult& HitResult);
};
