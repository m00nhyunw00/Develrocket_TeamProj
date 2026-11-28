#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "Data/TBClassChangeTypes.h"
#include "TBPlayerState.generated.h"

struct FGameplayTag;
struct FActiveGameplayEffectHandle;
struct FOnAttributeChangeData;
class UTBPlayerAttributeSet;
class UAbilitySystemComponent;
class UTBAbilitySystemComponent;
class UTBAttributeSetBase;
class UGameplayAbility;
class UGameplayEffect;
class UTBCharacterClassData;

DECLARE_MULTICAST_DELEGATE(FOnTBClassDataChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnTBClassChangeResult, ETBClassChangeResult);

USTRUCT(BlueprintType)
struct FTBMatchStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Match")
	int32 Kills = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Match")
	int32 Deaths = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Match")
	int32 MatchScore = 0;
};

UCLASS()
class DEVELROCKET_TEAMPROJ_API ATBPlayerState
	: public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ATBPlayerState();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	const FTBMatchStats& GetMatchStats() const { return MatchStats; }
	void AddMatchKill(int32 Points);
	void AddMatchDeath();
	void AddMatchScore(int32 Points);

	// 초기 데이터 적용 ----------------------

	void InitializeGameplayData();
	bool ResetForRespawn(FActiveGameplayEffectHandle DeathEffect);
	
	// 서버 전용 : 직업 변경 전 어빌리티 정리 --------------------
	
	void ResetGameplayDataForClassChange();

	// [서버] 승인된 직업으로 교체: GAS 정리 + 데이터 교체. 폰을 다시 만들어야 하면 true
	bool ApplyClassChange(const UTBCharacterClassData* NewClassData);

	// 현재 PlayerState의 사망 관련 이벤트를 한 번만 연결
	void InitializeDeathEvents();

	// 서버 혈액 회복 ----------------------

	void ApplyBloodRecovery();
	
	// 기능 태그에 해당하는 플레이어 공격력 조회 ----------------------
	
public:
	// 기능 태그에 해당하는 플레이어 공격력 조회
	bool GetAttackDamage(
		const FGameplayTag& AttackId, float& OutDamage) const;
	
	// 선택한 직업이 있으면 그것을, 없으면 기본 직업을 반환
	const UTBCharacterClassData* GetClassData() const;

	// 로비에서 실제로 고른 직업 (아직 안 골랐으면 nullptr)
	const UTBCharacterClassData* GetSelectedClassData() const;
	
	// 서버 전용: 데이터만 교체. 직업 변경은 ApplyClassChange 사용
	void SetSelectedClassData(const UTBCharacterClassData* NewClassData);
	
	// 리슨 서버 호스트는 OnRep을 받지 않으므로 HUD는 이 이벤트로 갱신
	FOnTBClassDataChanged OnClassDataChangedEvent;

	// 로비 직업 변경 신청 ----------------------
	// 흐름: 신청 → LobbyManager 결재 → 승인 시 ApplyClassChange → GameMode 재스폰 → 결과 통보

	// [로컬] 위젯에서 호출
	void RequestClassChange(const UTBCharacterClassData* ClassData);

	// [로컬] 결재 결과 (위젯이 구독해서 거절 사유 표시)
	FOnTBClassChangeResult OnClassChangeResult;

protected:
	// 기본 공격 피해량
	UPROPERTY(EditDefaultsOnly, Category = "GAS|PrimaryAttack", meta = (ClampMin = "0.0"))
	float PrimaryAttackDamage = 10.0f;

	// 우클릭을 공격으로 사용하는 캐릭터의 피해량
	UPROPERTY(EditDefaultsOnly, Category = "GAS|SecondaryAttack", meta = (ClampMin = "0.0"))
	float SecondaryAttackDamage = 0.0f;
	
	// 이동 중 타격하는 이동기의 피해량
	UPROPERTY(EditDefaultsOnly, Category = "GAS|Mobility", meta = (ClampMin = "0.0"))
	float MobilityAttackDamage = 0.0f;
	
	// F키 근접 공격 피해량
	UPROPERTY(EditDefaultsOnly, Category = "GAS|MeleeAttack", meta = (ClampMin = "0.0"))
	float MeleeAttackDamage = 20.0f;

	// 스킬1 피해량
	UPROPERTY(EditDefaultsOnly, Category = "GAS|Skill1", meta = (ClampMin = "0.0"))
	float Skill1Damage = 0.0f;

	// 스킬2 피해량
	UPROPERTY(EditDefaultsOnly, Category = "GAS|Skill2", meta = (ClampMin = "0.0"))
	float Skill2Damage = 0.0f;

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
	
	UPROPERTY(EditDefaultsOnly, Category = "GAS|Death")
	TSubclassOf<UGameplayEffect> DeathEffectClass;
	
	// 1단계 이전용: 기존 직업별 PlayerState BP에 DA를 하나씩 지정
	UPROPERTY(EditDefaultsOnly, Category = "Class")
	TObjectPtr<const UTBCharacterClassData> DefaultClassData;
	
private:
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match", meta = (AllowPrivateAccess = "true"))
	FTBMatchStats MatchStats;

	// 반복 초기화에 따른 자원 재설정 및 중복 부여 방지
	bool bGameplayDataInitialized = false;
	
	bool bDeathEventsBound = false;
	
private:
	void HandleBloodChanged(const FOnAttributeChangeData& Data);
	void HandleDeadTagChanged(const FGameplayTag Tag, int32 NewCount);
	
	UPROPERTY(ReplicatedUsing = OnRep_SelectedClassData)
	TObjectPtr<const UTBCharacterClassData> SelectedClassData;

	UFUNCTION()
	void OnRep_SelectedClassData();

	UFUNCTION(Server, Reliable)
	void ServerRequestClassChange(const UTBCharacterClassData* ClassData);

	UFUNCTION(Client, Reliable)
	void ClientClassChangeResult(ETBClassChangeResult Result);

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
	
	TSubclassOf<UGameplayEffect> GetDeathEffectClass() const
	{
		return DeathEffectClass;
	}
	
	// 직업 데이터 기반 조회 (DA 미지정 시 기존 필드 값 사용)
	float GetPrimaryAttackDamage() const;
	float GetMeleeAttackDamage() const;
	float GetPrimaryAttackRange() const;
	float GetPrimaryAttackInterval() const;

	// 공격 판정 디버그 표시 여부
	bool ShouldDrawPrimaryAttackTrace() const
	{
		return bDrawPrimaryAttackTrace;
	}

	// 직업 데이터 기반 조회 (DA 미지정 시 PlayerState 값 사용)
	TSubclassOf<UGameplayEffect> GetPrimaryAttackCooldownEffectClass() const;
};
