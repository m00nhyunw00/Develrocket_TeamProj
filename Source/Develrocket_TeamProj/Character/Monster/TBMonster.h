#pragma once

#include "CoreMinimal.h"
#include "Character/TBCharacterBase.h"
#include "TBMonster.generated.h"

class UGameplayAbility;
class UGameplayEffect;
class UTBAbilitySystemComponent;
class UTBMonsterAttributeSet;
class ATBMonster;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTBMonsterDied, ATBMonster*, Monster);

// 자체 ASC를 소유하는 GAS 기반 AI 몬스터
UCLASS()
class DEVELROCKET_TEAMPROJ_API ATBMonster : public ATBCharacterBase
{
	GENERATED_BODY()

public:
	ATBMonster(const FObjectInitializer& ObjectInitializer);

	virtual void PossessedBy(AController* NewController) override;

	UFUNCTION(BlueprintPure, Category = "Monster|Health")
	float GetHealth() const;

	UFUNCTION(BlueprintPure, Category = "Monster|Health")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintPure, Category = "Monster|State")
	bool IsDead() const { return bIsDead; }

	// 서버에서 피해 GE를 적용하는 공통 진입점; 클라이언트 RPC가 아닌 권한 검사 함수
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Monster|Damage")
	bool ApplyGASDamage(float Damage, AActor* DamageSource);

	UPROPERTY(BlueprintAssignable, Category = "Monster|State")
	FTBMonsterDied OnMonsterDied;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UTBAbilitySystemComponent> MonsterASC;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Combat")
	TSubclassOf<UGameplayAbility> MeleeAbilityClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UTBMonsterAttributeSet> MonsterAttributes;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Initialization")
	TSubclassOf<UGameplayEffect> InitialAttributesEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Abilities")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	// 기존 몬스터 BP에 사망 연출을 연결할 확장 지점
	UFUNCTION(BlueprintImplementableEvent, Category = "Monster|State", meta = (DisplayName = "On Death"))
	void ReceiveDeath();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	void InitializeMonsterAbilitySystem();
	void InitializeGameplayData();
	void Die();
	void HandleDeath();

	UFUNCTION()
	void OnRep_IsDead();

	UPROPERTY(ReplicatedUsing = OnRep_IsDead)
	bool bIsDead = false;

	bool bGameplayDataInitialized = false;
	bool bDeathHandled = false;
	FDelegateHandle OutOfHealthHandle;
};
