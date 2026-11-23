#pragma once

#include "CoreMinimal.h"
#include "Character/TBCharacterBase.h"
#include "Engine/DataTable.h"
#include "TBMonster.generated.h"

class UGameplayAbility;
class UTBMonsterSkillSet;
class UGameplayEffect;
class UTBMonsterAttributeSet;
class UTBMonsterMovementComponent;
class ATBMonster;
class UTBRotationComponent;
class UNiagaraSystem;
struct FAbilityEndedData;
struct FGameplayTag;

USTRUCT(BlueprintType)
struct FTBMonsterStatsRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1.0"))
	float MaxBlood = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	float MaxStagger = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0", Units = "cm/s"))
	float WalkSpeed = 750.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	float MeleeDamage = 20.0f;

	// Distance between actor origins. Zero disables melee attacks.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0", Units = "cm"))
	float MeleeRange = 150.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTBMonsterDied, ATBMonster*, Monster);

// 자체 ASC를 소유하는 GAS 기반 AI 몬스터
UCLASS()
class DEVELROCKET_TEAMPROJ_API ATBMonster : public ATBCharacterBase
{
	GENERATED_BODY()

public:
	ATBMonster(const FObjectInitializer& ObjectInitializer);

	virtual void PossessedBy(AController* NewController) override;
	virtual void OnPoolActivated_Implementation() override;
	virtual void OnPoolDeactivated_Implementation() override;
	virtual void OnPoolReady() override;
	void RestorePoolVisualState();

	UFUNCTION(BlueprintPure, Category = "Monster|State")
	bool IsDead() const;

	TSubclassOf<UGameplayAbility> GetMeleeAbilityClass() const { return MeleeAbilityClass; }
	float GetMeleeAttackRange() const { return MeleeAttackRange; }
	const UTBMonsterSkillSet* GetSkillSet() const { return SkillSet; }
	UTBMonsterMovementComponent* GetMonsterMovement() const;
	UTBRotationComponent* GetAttackRotationComponent() const { return AttackRotationComponent.Get(); }

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayBoomEffect(UNiagaraSystem* Effect, FVector_NetQuantize Location);

	// 근접 공격의 임시 고정 피해량 조회
	virtual bool GetAttackDamage(const FGameplayTag& AttackId, float& OutDamage) const override;

	// 서버에서 피해 GE를 적용하는 공통 진입점; 클라이언트 RPC가 아닌 권한 검사 함수
	virtual bool ApplyMeleeDamage(const FHitResult& HitResult, float Damage) override;

	UPROPERTY(BlueprintAssignable, Category = "Monster|State")
	FTBMonsterDied OnMonsterDied;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Combat")
	TObjectPtr<UTBMonsterSkillSet> SkillSet;

	virtual bool CanReceiveAttack(float Damage) const override;
	virtual TSubclassOf<UGameplayEffect> GetAttackDamageEffect() const override;
	virtual void OnAttackApplied(float Damage, AActor* DamageSource) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Combat")
	TSubclassOf<UGameplayAbility> MeleeAbilityClass;

	// Distance between actor origins. Zero disables melee attacks.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS|Combat", meta = (ClampMin = "0.0", Units = "cm"))
	float MeleeAttackRange = 150.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Rotation")
	TObjectPtr<UTBRotationComponent> AttackRotationComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UTBMonsterAttributeSet> MonsterAttributes;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Initialization")
	TSubclassOf<UGameplayEffect> InitialAttributesEffect;

	UPROPERTY(EditDefaultsOnly, Category = "GAS|Initialization")
	TSoftObjectPtr<UDataTable> MonsterStatsTable;

	// When empty, use the Blueprint name without its BP_ prefix.
	UPROPERTY(EditDefaultsOnly, Category = "GAS|Initialization")
	FName MonsterStatsRow;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Abilities")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	// 데미지를 받는 게임플레이 이펙트
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	// 사망 상태 태그를 부여할 블루프린트 Gameplay Effect
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|State")
	TSubclassOf<UGameplayEffect> DeathEffectClass;

private:
	void InitializeMonsterAbilitySystem();
	void InitializeGameplayData();
	const FTBMonsterStatsRow* FindMonsterStats() const;
	void ApplyMonsterStats(const FTBMonsterStatsRow& Stats);
	void HandleStaggerFull();
	void HandleAbilityEnded(const FAbilityEndedData& EndedData);
	void HandleDeadTagChanged(const FGameplayTag Tag, int32 NewCount);
	void Die();
	void HandleDeath();
	
	bool bGameplayDataInitialized = false;
	float MeleeDamage = 20.0f;
	bool bDeathHandled = false;
	bool bReturnedToPool = false;
	bool bPausedBrainForStagger = false;
	FDelegateHandle OutOfBloodHandle;
	FDelegateHandle StaggerFullHandle;
	FDelegateHandle AbilityEndedHandle;
	FDelegateHandle DeadTagChangedHandle;
};
