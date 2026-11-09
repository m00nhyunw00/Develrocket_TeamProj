#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "TBPlayerState.generated.h"

class UTBPlayerAttributeSet;
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
	// 기본 공격 피해량
	UPROPERTY(EditDefaultsOnly, Category = "GAS|PrimaryAttack", meta = (ClampMin = "0.0"))
	float PrimaryAttackDamage = 10.0f;

	// 기본 공격 사거리(cm)
	UPROPERTY(EditDefaultsOnly, Category = "GAS|PrimaryAttack", meta = (ClampMin = "1.0"))
	float PrimaryAttackRange = 10000.0f;

	// 서버 공격 판정 표시
	UPROPERTY(EditDefaultsOnly, Category = "GAS|PrimaryAttack")
	bool bDrawPrimaryAttackTrace = true;
	
	// 일반 공격 간격(초)
	UPROPERTY(EditDefaultsOnly, Category = "GAS|PrimaryAttack", meta = (ClampMin = "0.01"))
	float PrimaryAttackInterval = 0.1f;
	
	// GAS 구성 ----------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UTBAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UTBPlayerAttributeSet> PlayerAttributeSet;

	// 초기 데이터 ----------------------

	UPROPERTY(EditDefaultsOnly, Category = "GAS|Initialization")
	TSubclassOf<UGameplayEffect> InitialBloodEffect;

	UPROPERTY(EditDefaultsOnly, Category = "GAS|Abilities")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	// 회복 데이터 ----------------------

	UPROPERTY(EditDefaultsOnly, Category = "GAS|Blood")
	TSubclassOf<UGameplayEffect> BloodRecoveryEffect;
	
	// 플레이어 피해 효과 ----------------------
	UPROPERTY(EditDefaultsOnly, Category = "GAS|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	// 일반 공격 쿨다운 효과 ----------------------
	UPROPERTY(EditDefaultsOnly, Category = "GAS|PrimaryAttack")
	TSubclassOf<UGameplayEffect> PrimaryAttackCooldownEffectClass;
	
private:
	// 반복 초기화에 따른 자원 재설정 및 중복 부여 방지
	bool bGameplayDataInitialized = false;
	
public:
	// 게임플레이 데이터 초기화 완료 여부
	bool IsGameplayDataInitialized() const
	{
		return bGameplayDataInitialized;
	}

	// 플레이어 피해 효과 조회
	TSubclassOf<UGameplayEffect> GetDamageEffectClass() const
	{
		return DamageEffectClass;
	}
	
	// 기본 공격 피해량 조회
	float GetPrimaryAttackDamage() const
	{
		return PrimaryAttackDamage;
	}

	// 기본 공격 사거리 조회
	float GetPrimaryAttackRange() const
	{
		return PrimaryAttackRange;
	}

	// 공격 판정 디버그 표시 여부
	bool ShouldDrawPrimaryAttackTrace() const
	{
		return bDrawPrimaryAttackTrace;
	}
	
	float GetPrimaryAttackInterval() const
	{
		return PrimaryAttackInterval;
	}

	TSubclassOf<UGameplayEffect> GetPrimaryAttackCooldownEffectClass() const
	{
		return PrimaryAttackCooldownEffectClass;
	}
};