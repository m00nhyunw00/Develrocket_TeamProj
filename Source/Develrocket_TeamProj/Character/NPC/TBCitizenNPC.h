#pragma once

#include "CoreMinimal.h"
#include "Character/TBCharacterBase.h"
#include "Interface/TBInteractable.h"
#include "TBDefine.h"
#include "TBCitizenNPC.generated.h"

class UAnimMontage;
class UGameplayAbility;
class ATBCitizenNPC;

// 시체 유지 시간이 끝났음을 알림 → SpawnManager가 구독해서 풀로 반환
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTBCitizenCorpseExpired, ATBCitizenNPC*, Citizen);

UCLASS()
class DEVELROCKET_TEAMPROJ_API ATBCitizenNPC : public ATBCharacterBase, public ITBInteractable
{
	GENERATED_BODY()

public:
	ATBCitizenNPC(const FObjectInitializer& ObjectInitializer);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	// 피격 (ITBHitInterface) ----------------------
	virtual bool ReceiveAttackHit_Implementation(const FGameplayTag& DamageType, float Damage, AActor* DamageSource, const FHitResult& HitResult) override;

	UFUNCTION(BlueprintPure, Category = "Citizen|Health")
	bool IsInjured() const { return bIsInjured; }         // ABP에서 읽음

	// 상호작용 계약 ----------------------
	virtual bool CanInteract_Implementation(AActor* InteractInstigator) const override;   // Idle일 때만
	virtual void BeginInteract_Implementation(AActor* InteractInstigator) override;       // [서버] 흡혈 시작 → Draining
	virtual void CancelInteract_Implementation(AActor* InteractInstigator) override;      // [서버] 흡혈 취소 → Idle
	virtual void Interact_Implementation(AActor* InteractInstigator) override;            // [서버] 흡혈 완료 → Heal + 사망
	virtual void InteractLocal_Implementation(AActor* InteractInstigator) override;
	virtual FText GetInteractPrompt_Implementation() const override;
	virtual UAnimMontage* GetInteractMontage_Implementation() const override;
	virtual UAnimMontage* GetInteractFirstPersonMontage_Implementation() const override;
	virtual bool ShouldResolveOnStart_Implementation() const override;

	// 풀링 계약 ----------------------
	virtual void OnPoolActivated_Implementation() override;       // [서버] 풀에서 꺼낼 때 살아있는 상태로 복구
	virtual void OnPoolDeactivated_Implementation() override;     // [서버] 풀로 넣기 직전 정리

	// 시체 유지 종료 알림 (SpawnManager 구독)
	UPROPERTY(BlueprintAssignable, Category = "Citizen")
	FTBCitizenCorpseExpired OnCitizenCorpseExpired;

protected:
	virtual void BeginPlay() override;
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
    float FeedAlignDistance = 80.f;                       // 플레이어 정면 기준 거리 (캡슐 반지름 합 이상)
    
    UPROPERTY(EditDefaultsOnly, Category = "Interaction|Align", meta = (Units = "deg"))
    float FeedAlignYawOffset = 180.f;                     // 180 = 플레이어를 마주봄, 0 = 등을 보임
    
    void AlignToFeeder(const AActor* Feeder);             // [서버] 위치·방향 맞춤
	
	// 체력·출혈 ----------------------
	UPROPERTY(EditDefaultsOnly, Category = "Citizen|Health", meta = (ClampMin = "1.0"))
	float MaxHealth = 30.f;

	UPROPERTY(EditDefaultsOnly, Category = "Citizen|Health", meta = (ClampMin = "0.0"))
	float BleedDamagePerSecond = 4.f;

	UPROPERTY(EditDefaultsOnly, Category = "Citizen|Health", meta = (ClampMin = "0.0", Units = "cm/s"))
	float InjuredWalkSpeed = 200.f;

	UPROPERTY(Replicated)
	bool bIsInjured = false;                              // [복제] ABP 부상 애니메이션 전환용

	float CurrentHealth = 0.f;                            // [서버 전용]
	FTimerHandle BleedTimer;

	void ApplyCitizenDamage(float Amount);                // [서버] 체력 감소의 유일한 입구
	void HandleBleedTick();                               // [서버] 1초마다 출혈
	void ResetHealth();                                   // [서버] 스폰·풀 재사용 시 초기화

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

private:
	void StartDeathSequence();                            // [서버] 사망 GA 실행 + 시체 타이머
	void HandleCorpseExpired();                           // [서버] 시체 유지 종료
	void RestoreAliveState();                             // [모든 머신] 충돌·이동 복구

	TWeakObjectPtr<AActor> DrainInstigator;               // 지금 나를 흡혈 중인 플레이어
	FTimerHandle CorpseTimer;
};