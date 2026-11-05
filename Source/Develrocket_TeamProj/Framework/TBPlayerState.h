#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "TBPlayerState.generated.h"

class UAbilitySystemComponent;
class UTBAbilitySystemComponent;
class UTBAttributeSetBase;
class UGameplayAbility;
class UGameplayEffect;

UCLASS()
class DEVELROCKET_TEAMPROJ_API ATBPlayerState
	: public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ATBPlayerState();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// 초기 데이터 적용 ----------------------

	void InitializeGameplayData();

	// 서버 혈액 회복 ----------------------

	void ApplyBloodRecovery();

protected:
	// GAS 구성 ----------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UTBAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UTBAttributeSetBase> PlayerAttributeSet;

	// 초기 데이터 ----------------------

	UPROPERTY(EditDefaultsOnly, Category = "GAS|Initialization")
	TSubclassOf<UGameplayEffect> InitialBloodEffect;

	UPROPERTY(EditDefaultsOnly, Category = "GAS|Abilities")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	// 회복 데이터 ----------------------

	UPROPERTY(EditDefaultsOnly, Category = "GAS|Blood")
	TSubclassOf<UGameplayEffect> BloodRecoveryEffect;

private:
	// 반복 초기화에 따른 자원 재설정 및 중복 부여 방지
	bool bGameplayDataInitialized = false;
};