#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "Interface/TBPoolInterface.h"
#include "Interface/TBHitInterface.h"
#include "TBCharacterBase.generated.h"

class UTBAbilitySystemComponent;
class UAbilitySystemComponent;
class UTBBloodBarComponent;
class UGameplayEffect;

UCLASS(Abstract)
class DEVELROCKET_TEAMPROJ_API ATBCharacterBase
	: public ACharacter, public IAbilitySystemInterface, public ITBHitInterface, public ITBPoolInterface
{
	GENERATED_BODY()

public:
	ATBCharacterBase(const FObjectInitializer& ObjectInitializer);

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
	UTBAbilitySystemComponent* GetTBAbilitySystemComponent() const;
	
	// 피격 대상에 설정된 혈액 연출 태그 반환
	FGameplayTag GetBloodImpactCueTag() const
	{
		return BloodImpactCueTag;
	}

	// ASC 연결 상태와 상태 태그에 따른 이동 차단 여부 조회
	UFUNCTION(BlueprintPure, Category = "Character|State")
	bool IsMovementBlocked() const;

	// 사망·스턴·흡혈 상태에 따른 행동 입력 차단 여부 조회
	UFUNCTION(BlueprintPure, Category = "Character|State")
	bool IsActionBlocked() const;

	UFUNCTION(BlueprintCallable, Category = "Character|Combat")
	virtual void StartPrimaryAttack();

	UFUNCTION(BlueprintCallable, Category = "Character|Combat")
	virtual void StopPrimaryAttack();

	UFUNCTION(BlueprintCallable, Category = "Character|Combat")
	virtual void StartSkill();
	
	UFUNCTION(BlueprintCallable, Category = "Character|Combat")
	virtual void StopSkill();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	TObjectPtr<UTBBloodBarComponent> BloodBarComponent;

	virtual void UnPossessed() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 전달받은 ASC의 Owner와 현재 캐릭터의 Avatar 역할 연결
	void InitializeAbilitySystem(UAbilitySystemComponent* InASC, AActor* InOwnerActor);

	// Avatar 연결 해제 및 외부 소유 ASC 참조 정리
	void UninitializeAbilitySystem();

	// 서버에서 공통 피해 GE 적용과 공격자별 HP 바 대상 선택을 처리한다.
	virtual bool ReceiveAttackHit_Implementation(const FGameplayTag& DamageType, float Damage, AActor* DamageSource, const FHitResult& HitResult) override;
	// 자식 클래스에서 피격 가능 상태와 사용할 GE, 피격 후 반응을 지정한다.
	virtual bool CanReceiveAttack(float Damage) const;
	virtual TSubclassOf<UGameplayEffect> GetAttackDamageEffect() const;
	virtual void OnAttackApplied(float Damage, AActor* DamageSource);
	// false이면 공격자가 아닌 피격 대상의 ASC로 GE 컨텍스트를 만든다.
	virtual bool UseSourceASCForAttackEffect() const { return true; }
	
	// 모든 캐릭터에서 공유하는 피격 데칼 제거 구현
	virtual void ClearHitImpactDecals_Implementation() override;

	// 몬스터는 직접 생성하고 플레이어는 PlayerState에서 연결하는 공통 ASC 참조
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UTBAbilitySystemComponent> AbilitySystemComponent;
	
	// 캐릭터 BP에서 지정할 혈액 피격 연출 태그
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|GameplayCue",
		meta = (Categories = "GameplayCue"))
	FGameplayTag BloodImpactCueTag;
};
