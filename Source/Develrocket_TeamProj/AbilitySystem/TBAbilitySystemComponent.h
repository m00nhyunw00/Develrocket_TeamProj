#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "TBAbilitySystemComponent.generated.h"

class UGameplayAbility;

UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	// 기본 Ability 관리 ----------------------

	void AddCharacterAbilities(const TArray<TSubclassOf<UGameplayAbility>>& StartupAbilities);

	void RemoveCharacterAbilities();

	// Ability 활성화 ----------------------

	bool TryActivateAbilityByTag(const FGameplayTag& AbilityTag);

private:
	// 서버에서 직접 부여한 기본 Ability의 핸들 기록
	UPROPERTY(Transient)
	TArray<FGameplayAbilitySpecHandle> StartupAbilityHandles;
};