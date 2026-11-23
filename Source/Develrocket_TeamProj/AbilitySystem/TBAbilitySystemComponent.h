#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "TBAbilitySystemComponent.generated.h"

class UGameplayAbility;

DECLARE_MULTICAST_DELEGATE(FOnPrimaryShotFired);

UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()
	
public:
	FOnPrimaryShotFired OnPrimaryShotFired;

public:
	// 기본 Ability 관리 ----------------------

	void AddCharacterAbilities(const TArray<TSubclassOf<UGameplayAbility>>& StartupAbilities);

	void RemoveCharacterAbilities();

	// Ability 활성화 ----------------------

	bool TryActivateAbilityByTag(const FGameplayTag& AbilityTag);
	
	// 기능 태그에 해당하는 어빌리티의 입력 시작·해제 전달
	void AbilityInputTagPressed(const FGameplayTag& InputTag);
	void AbilityInputTagReleased(const FGameplayTag& InputTag);

private:
	// 서버에서 직접 부여한 기본 Ability의 핸들 기록
	UPROPERTY(Transient)
	TArray<FGameplayAbilitySpecHandle> StartupAbilityHandles;
};