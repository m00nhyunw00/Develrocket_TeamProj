#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "TBMonsterGameplayEffects.generated.h"

UCLASS(Blueprintable)
class DEVELROCKET_TEAMPROJ_API UTBGameplayEffect_MonsterInitialize : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UTBGameplayEffect_MonsterInitialize();
};

UCLASS(Blueprintable)
class DEVELROCKET_TEAMPROJ_API UTBGameplayEffect_MonsterDamage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UTBGameplayEffect_MonsterDamage();
};

UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBGameplayEffect_MonsterDeath : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UTBGameplayEffect_MonsterDeath();
};
