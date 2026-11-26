#pragma once

#include "CoreMinimal.h"
#include "Character/TBCharacterBase.h"
#include "Interface/TBInteractInterface.h"
#include "GameplayEffectTypes.h"
#include "TBDefine.h"
#include "TBCitizen.generated.h"

class UAnimMontage;
class UGameplayAbility;
class UGameplayEffect;
class UTBCitizenAttributeSet;
class UTBCitizenAppearanceData;

// 놀람 몽타주와 애니 시선 보정값 묶음
USTRUCT(BlueprintType)
struct FTBStartleAnim
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimMontage> Montage;

	UPROPERTY(EditAnywhere, meta = (Units = "deg", ClampMin = "-180.0", ClampMax = "180.0"))
	float LookYawOffset = 0.f;                            // 애니 시선이 몸 정면에서 틀어진 각도 (+ = 오른쪽), 위협 방향 회전 보정용
};

UCLASS()
class DEVELROCKET_TEAMPROJ_API ATBCitizen : public ATBCharacterBase, public ITBInteractInterface
{
	GENERATED_BODY()

public:
	ATBCitizen(const FObjectInitializer& ObjectInitializer);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	// 피격 (ITBHitInterface) ----------------------

	UFUNCTION(BlueprintPure, Category = "Citizen|Health")
	bool IsInjured() const;                               // ABP에서 읽음 (State.Bleeding 태그 기준)

	// 상호작용 계약 ----------------------
	virtual bool CanInteract_Implementation(AActor* InteractInstigator) const override;   // Idle일 때만
	virtual void BeginInteract_Implementation(AActor* InteractInstigator) override;       // [서버] 흡혈 시작 → Draining
	virtual void CancelInteract_Implementation(AActor* InteractInstigator) override;      // [서버] 흡혈 취소 → Idle
	virtual void Interact_Implementation(AActor* InteractInstigator) override;            // [서버] 흡혈 완료 → Heal + 사망
	virtual void InteractLocal_Implementation(AActor* InteractInstigator) override;
	virtual FText GetInteractPrompt_Implementation() const override;
	virtual UAnimMontage* GetInteractMontage_Implementation() const override;
	virtual UAnimMontage* GetInteractFirstPersonMontage_Implementation() const override;
	virtual FGameplayTag GetInteractionAbilityTag_Implementation(AActor* InteractInstigator) const override;

	// 풀링 계약 ----------------------
	virtual void OnPoolActivated_Implementation() override;       // [서버] 풀에서 꺼낼 때 살아있는 상태로 복구
	virtual void OnPoolDeactivated_Implementation() override;     // [서버] 풀로 넣기 직전 정리
	virtual void OnPoolReady() override;
	
	ETBCitizenState GetCitizenState() const { return CitizenState; }
	float PlayStartle(float& OutLookYawOffset);           // [서버] 놀람 몽타주 선택·재생, 재생 길이와 시선 보정값 반환
	void SetRotationLocked(bool bLocked);                 // 놀람 재생 중 회전 고정 여부 전환

	// 걷기/뛰기 -------------------------
	void SetFleeing(bool bNewFleeing);                    // [서버] 도주 여부에 따른 걷기·뛰기 속도 전환

	
protected:
	virtual bool CanReceiveAttack(float Damage) const override;
	virtual TSubclassOf<UGameplayEffect> GetAttackDamageEffect() const override;
	virtual void OnAttackApplied(float Damage, AActor* DamageSource) override;
	// Citizen 피해 GE는 기존과 같이 Citizen 자신의 ASC 컨텍스트를 사용한다.
	virtual bool UseSourceASCForAttackEffect() const override { return false; }
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PossessedBy(AController* NewController) override;

	// 상호작용 데이터 ----------------------
	UPROPERTY(EditAnywhere, Category = "Interaction")
	FText InteractPrompt;

	UPROPERTY(EditDefaultsOnly, Category = "Interaction|Animation")
	TObjectPtr<UAnimMontage> FeedMontage;                 // 플레이어 3인칭 (서버의 완료 시점 기준)

	UPROPERTY(EditDefaultsOnly, Category = "Interaction|Animation")
	TObjectPtr<UAnimMontage> FeedFirstPersonMontage;      // 플레이어 1인칭 (보여주기용, 길이를 3인칭과 맞출 것)

	UPROPERTY(EditDefaultsOnly, Category = "Interaction|Animation")
	TObjectPtr<UAnimMontage> VictimMontage;               // 시민이 흡혈당하는 몽타주

	UPROPERTY(EditDefaultsOnly, Category = "Interaction", meta = (Units = "cm"))
	float MaxDrainDistance = 250.f;
	
	// 흡혈 시작 시 시민을 플레이어 정면 고정 위치로 맞춤 (Feed·Victim 몽타주가 맞물리도록)
	UPROPERTY(EditDefaultsOnly, Category = "Interaction|Align", meta = (Units = "cm"))
	float FeedAlignDistance = 72.f;                       // 애니 제작 기준 (rig.py OY=72)

	UPROPERTY(EditDefaultsOnly, Category = "Interaction|Align", meta = (Units = "cm"))
	float FeedAlignSideOffset = 12.f;                     // + = 플레이어 오른쪽 (rig.py OX=-12)	
	
	UPROPERTY(EditDefaultsOnly, Category = "Interaction|Align", meta = (Units = "deg"))
	float FeedAlignYawOffset = 180.f;                     // 180 = 플레이어를 마주봄, 0 = 등을 보임
	
	void AlignToFeeder(const AActor* Feeder);             // [서버] 위치·방향 맞춤
	
	// 혈액(체력)·출혈 (GAS) ----------------------
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UTBCitizenAttributeSet> CitizenAttributes;

	UPROPERTY(EditDefaultsOnly, Category = "Citizen|Health", meta = (ClampMin = "1.0"))
	float InitialMaxBlood = 30.f;                         // 스폰·재사용 시 MaxBlood·Blood 값

	UPROPERTY(EditDefaultsOnly, Category = "Citizen|Health")
	TSubclassOf<UGameplayEffect> DamageEffectClass;       // Instant, Blood += SetByCaller(Data.Damage)

	UPROPERTY(EditDefaultsOnly, Category = "Citizen|Health")
	TSubclassOf<UGameplayEffect> BleedEffectClass;        // Infinite·Period 1s, Blood += SetByCaller(Data.Damage), State.Bleeding 부여

	UPROPERTY(EditDefaultsOnly, Category = "Citizen|Health", meta = (ClampMin = "0.0"))
	float BleedDamagePerSecond = 4.f;

	UPROPERTY(EditDefaultsOnly, Category = "Citizen|Health", meta = (ClampMin = "0.0", Units = "cm/s"))
	float InjuredWalkSpeed = 200.f;

	void ResetVitals();                                   // [서버] 혈액 가득 + 출혈 제거 (스폰·풀 재사용)
	void StartBleeding();                                 // [서버] 첫 피격 시 출혈 GE 적용
	void StopBleeding();                                  // [서버] 출혈 GE 제거 (State.Bleeding도 함께 제거)
	void HandleOutOfBlood();                              // [서버] 혈액 0 → 사망
	void HandleBleedingTagChanged(const FGameplayTag Tag, int32 NewCount);   // [서버] 부상 감속

	// 사망 ----------------------
	UPROPERTY(EditDefaultsOnly, Category = "Citizen|Death")
	TSubclassOf<UGameplayAbility> DeathAbilityClass;      // GA_Citizen_Death (부모: TBGameplayAbility_Death)

	UPROPERTY(EditDefaultsOnly, Category = "Citizen|Death", meta = (ClampMin = "0.0", Units = "s"))
	float CorpseHoldSeconds = 5.f;                        // 사망 몽타주가 끝난 뒤 시체 유지 시간 (n초)

	// 상태 ----------------------
	UPROPERTY(ReplicatedUsing = OnRep_CitizenState)
	ETBCitizenState CitizenState = ETBCitizenState::Idle;

	UFUNCTION()
	void OnRep_CitizenState();                            // [모든 머신] 보이는 처리만

	void SetCitizenState(ETBCitizenState NewState);       // [서버] 상태 변경의 유일한 입구
	
	UPROPERTY(EditDefaultsOnly, Category = "Citizen|Animation")
	TArray<FTBStartleAnim> StartleAnims;                  // 무작위로 고를 놀람 몽타주 후보

	UPROPERTY(EditDefaultsOnly, Category = "Citizen|Appearance")
	TObjectPtr<UTBCitizenAppearanceData> AppearanceCandidates;   // 외형 메시 후보 데이터

	UPROPERTY(ReplicatedUsing = OnRep_Appearance)
	TSoftObjectPtr<USkeletalMesh> CurrentAppearance;      // [서버] 선택한 외형 메시, 클라는 OnRep으로 적용
	
	UFUNCTION()
	void OnRep_Appearance();
	
	void ChooseAppearance();
	void ApplyAppearance();

	UPROPERTY(EditDefaultsOnly, Category = "Citizen|Movement", meta=(ClampMin = "0.0", Units = "cm/s"))
	float WalkSpeed = 180.f;                              // 평상시 순찰·진정 후 걷기 속도

	UPROPERTY(EditDefaultsOnly, Category = "Citizen|Movement", meta=(ClampMin = "0.0", Units = "cm/s"))
	float RunSpeed = 500.f;                               // 도주 중 뛰기 속도
private:
	void StartDeathSequence();                            // [서버] 사망 GA 실행 + 시체 타이머
	void HandleCorpseExpired();                           // [서버] 시체 유지 종료
	void RestoreAliveState();                             // [모든 머신] 충돌·이동 복구

	TWeakObjectPtr<AActor> DrainInstigator;               // 지금 나를 흡혈 중인 플레이어
	FTimerHandle CorpseTimer;

	FActiveGameplayEffectHandle BleedEffectHandle;        // 현재 적용 중인 출혈 GE
	FDelegateHandle OutOfBloodHandle;
	FDelegateHandle BleedingTagHandle;
	
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayStartle(uint8 MontageIndex);
	
	int32 LastStartleIndex = INDEX_NONE;                  // 같은 놀람 몽타주 연속 재생 방지

	void UpdateMoveSpeed();                               // [서버] 부상·도주 상태 기준 이동 속도 일괄 갱신
	bool bFleeing = false;
};
