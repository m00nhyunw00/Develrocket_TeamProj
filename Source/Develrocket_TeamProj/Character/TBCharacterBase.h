#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"  
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Interface/TBPoolInterface.h"
#include "Interface/TBHitInterface.h"
#include "Interface/TBCombatInterface.h"
#include "TBCharacterBase.generated.h"

struct FActiveGameplayEffectHandle;
class ATBProjectile;
struct FTBAttackData;
class UTBMeleeTraceComponent;
class UTBAbilitySystemComponent;
class UAbilitySystemComponent;
class UTBBloodBarComponent;
class UTBOutlineComponent;
class UTBHealComponent;
class UGameplayEffect;

UCLASS(Abstract)
class DEVELROCKET_TEAMPROJ_API ATBCharacterBase
	: public ACharacter, public IAbilitySystemInterface, public ITBHitInterface, public ITBPoolInterface, public ITBCombatInterface
{
	GENERATED_BODY()

public:
	// 생성 및 공통 ASC 접근 ----------------------

	ATBCharacterBase(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "Outline")
	void SetCharacterOutlineColor(float Red, float Green, float Blue);

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
	UTBAbilitySystemComponent* GetTBAbilitySystemComponent() const;
	
	// 무적 상태 처리 ----------------------
	
	// 자신에게 무적 GE를 적용하고 이번 효과의 핸들을 반환
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Combat|Invulnerability")
	FActiveGameplayEffectHandle ApplyInvulnerability();

	// 전달받은 핸들에 해당하는 무적만 해제
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Combat|Invulnerability")
	void RemoveInvulnerability(FActiveGameplayEffectHandle Handle);

	// 서버의 3인칭 몽타주에서 발생한 무적 구간 시작·종료 처리
	void SetMontageInvulnerability(const FBranchingPointNotifyPayload& Payload, const UAnimNotifyState* Window, bool bBegin);
	
	// 공통 행동 상태 조회 ----------------------

	// ASC 연결 상태와 상태 태그에 따른 이동 차단 여부 조회
	UFUNCTION(BlueprintPure, Category = "Character|State")
	bool IsMovementBlocked() const;

	// 사망·스턴·흡혈 상태에 따른 행동 입력 차단 여부 조회
	UFUNCTION(BlueprintPure, Category = "Character|State")
	bool IsActionBlocked() const;

	// 공격력 및 피해 전달 ----------------------

	// 기능별 공격력 제공
	virtual bool GetAttackDamage(const FGameplayTag& AttackId, float& OutDamage) const;

	// 확정된 피해량으로 공격 방식별 명중 처리
	virtual bool ApplyHitscanDamage(const FHitResult& HitResult, float Damage);

	virtual bool ApplyMeleeDamage(const FHitResult& HitResult, float Damage);

	virtual bool ApplyProjectileDamage(const FHitResult& HitResult, float Damage);

	// 투사체 생성 및 라인 트레이스 ----------------------

	// 서버 투사체 생성; 실제 발사는 구독 완료 후 수행
	ATBProjectile* SpawnAttackProjectile(TSubclassOf<ATBProjectile> ProjectileClass, const FTransform& SpawnTransform);

	// 자신을 제외한 단일 라인 트레이스 및 충돌 정보 반환
	bool LineTraceByChannel(FHitResult& OutHit, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel) const;

	// 근접 판정 컴포넌트 및 ANS 요청 처리 ----------------------

	UTBMeleeTraceComponent* GetMeleeTraceComponent() const;

	// 소켓 검사에 사용할 메시 반환
	virtual UPrimitiveComponent* GetMeleeTraceMesh() const;

	virtual void BeginMeleeTrace(USkeletalMeshComponent* SourceMesh) override;

	virtual void EndMeleeTrace(USkeletalMeshComponent* SourceMesh) override;
	
	// 피격 연출 조회 ----------------------

	// 피격 대상에 설정된 혈액 연출 태그 반환
	FGameplayTag GetBloodImpactCueTag() const
	{
		return BloodImpactCueTag;
	}
	
	// 공격 확정 시 강화 소비 ----------------------
	
	virtual bool ConsumeAttackEnhancement(
		const FGameplayTag& AttackId, float& InOutDamage)
	{
		return false;
	}

protected:
	// 캐릭터 공통 투사체 피격 설정
	virtual void BeginPlay() override;

	// 빙의 해제 및 종료 처리 ----------------------

	virtual void UnPossessed() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// GAS 연결 및 해제 ----------------------

	// 전달받은 ASC의 Owner와 현재 캐릭터의 Avatar 역할 연결
	void SetCharacterOutlineSuppressed(bool bSuppressed);
	void InitializeAbilitySystem(UAbilitySystemComponent* InASC, AActor* InOwnerActor);

	// Avatar 연결 해제 및 외부 소유 ASC 참조 정리
	void UninitializeAbilitySystem();

	// 몬스터는 직접 생성하고 플레이어는 PlayerState에서 연결하는 공통 ASC 참조
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UTBAbilitySystemComponent> AbilitySystemComponent;
	
	// 이 캐릭터의 무적 부여에 사용할 공용 GE
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Invulnerability")
	TSubclassOf<UGameplayEffect> InvulnerabilityEffectClass;

	// 공통 피격 처리 및 자식 클래스 확장 ----------------------

	// 공통 피해 GameplayEffect 적용
	virtual bool ReceiveAttackHit_Implementation(float Damage, AActor* DamageSource, const FHitResult& HitResult) override;

	// 자식 클래스별 피격 조건·피해 GE·피격 후 반응 지정
	virtual bool CanReceiveAttack(float Damage) const;
	virtual TSubclassOf<UGameplayEffect> GetAttackDamageEffect() const;
	virtual void OnAttackApplied(float Damage, AActor* DamageSource);

	// false일 때 피격 대상의 ASC로 GE 컨텍스트 생성
	virtual bool UseSourceASCForAttackEffect() const { return true; }
	
	// 피해량 보정 ----------------------
	
	virtual float ModifyIncomingAttackDamage(float Damage) const
	{
		return Damage;
	}

	// 공통 피격 연출 ----------------------

	// 모든 캐릭터에서 공유하는 피격 데칼 제거 구현
	virtual void ClearHitImpactDecals_Implementation() override;

	// 캐릭터 BP에서 지정할 혈액 피격 연출 태그
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|GameplayCue", meta = (Categories = "GameplayCue"))
	FGameplayTag BloodImpactCueTag;

	// 공통 전투 및 UI 컴포넌트 ----------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UTBMeleeTraceComponent> MeleeTraceComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	TObjectPtr<UTBBloodBarComponent> BloodBarComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Outline")
	TObjectPtr<UTBOutlineComponent> OutlineComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heal")
	TObjectPtr<UTBHealComponent> HealComponent;
	
private:
	// 같은 몽타주를 다시 재생해도 이전 재생의 종료 알림과 구분
	using FInvulnerabilityWindowKey = TPair<int32, const UAnimNotifyState*>;

	TMap<FInvulnerabilityWindowKey, FActiveGameplayEffectHandle> InvulnerabilityWindows;

	void CloseInvulnerabilityWindow(const FInvulnerabilityWindowKey& Key);
};
