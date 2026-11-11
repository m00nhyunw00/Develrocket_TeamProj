#pragma once

#include "CoreMinimal.h"
#include "Character/TBCharacterBase.h"
#include "TBMonster.generated.h"

class UGameplayAbility;
class UGameplayEffect;
class UTBMonsterAttributeSet;
class UTBGameplayAbility_Monster_Leap;
class ATBMonster;
struct FAbilityEndedData;
struct FGameplayTag;

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
	void RestorePoolVisualState();

	UFUNCTION(BlueprintPure, Category = "Monster|State")
	bool IsDead() const;

	TSubclassOf<UGameplayAbility> GetMeleeAbilityClass() const { return MeleeAbilityClass; }
	TSubclassOf<UTBGameplayAbility_Monster_Leap> GetLeapAbilityClass() const { return LeapAbilityClass; }

	// 서버에서 피해 GE를 적용하는 공통 진입점; 클라이언트 RPC가 아닌 권한 검사 함수
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Monster|Damage")
	bool ApplyDamage(float Damage, AActor* DamageSource);

	UPROPERTY(BlueprintAssignable, Category = "Monster|State")
	FTBMonsterDied OnMonsterDied;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Combat")
	TSubclassOf<UGameplayAbility> MeleeAbilityClass;

	// 몬스터 BP에서 지정하는 선택형 도약 어빌리티
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Combat")
	TSubclassOf<UTBGameplayAbility_Monster_Leap> LeapAbilityClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UTBMonsterAttributeSet> MonsterAttributes;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Initialization")
	TSubclassOf<UGameplayEffect> InitialAttributesEffect;

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
	void HandleStaggerFull();
	void HandleAbilityEnded(const FAbilityEndedData& EndedData);
	void HandleDeadTagChanged(const FGameplayTag Tag, int32 NewCount);
	void Die();
	void HandleDeath();
	virtual bool ReceiveAttackHit_Implementation(const FGameplayTag& DamageType, float Damage, AActor* DamageSource, const FHitResult& HitResult) override;
	
	bool bGameplayDataInitialized = false;
	bool bDeathHandled = false;
	bool bPausedBrainForStagger = false;
	FDelegateHandle OutOfBloodHandle;
	FDelegateHandle StaggerFullHandle;
	FDelegateHandle AbilityEndedHandle;
	FDelegateHandle DeadTagChangedHandle;
};
