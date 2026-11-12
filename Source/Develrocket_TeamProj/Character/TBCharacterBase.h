#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "Interface/TBPoolable.h"
#include "Interface/TBHitInterface.h"
#include "TBCharacterBase.generated.h"

class UTBAbilitySystemComponent;
class UAbilitySystemComponent;

UCLASS(Abstract)
class DEVELROCKET_TEAMPROJ_API ATBCharacterBase
	: public ACharacter, public IAbilitySystemInterface, public ITBHitInterface, public ITBPoolable
{
	GENERATED_BODY()

public:
	ATBCharacterBase(const FObjectInitializer& ObjectInitializer);

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
	UTBAbilitySystemComponent* GetTBAbilitySystemComponent() const;
	
	FGameplayTag GetCharacterTypeTag() const
	{
		return CharacterTypeTag;
	}
	
	// 피격 대상에 설정된 혈액 연출 태그 반환
	FGameplayTag GetBloodImpactCueTag() const
	{
		return BloodImpactCueTag;
	}

	// ASC 연결 상태와 상태 태그에 따른 이동 차단 여부 조회
	UFUNCTION(BlueprintPure, Category = "Character|State")
	bool IsMovementBlocked() const;

	UFUNCTION(BlueprintCallable, Category = "Character|Combat")
	virtual void StartPrimaryAttack();

	UFUNCTION(BlueprintCallable, Category = "Character|Combat")
	virtual void StopPrimaryAttack();

	UFUNCTION(BlueprintCallable, Category = "Character|Combat")
	virtual void StartSkill();
	
	UFUNCTION(BlueprintCallable, Category = "Character|Combat")
	virtual void StopSkill();

protected:
	virtual void UnPossessed() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 전달받은 ASC의 Owner와 현재 캐릭터의 Avatar 역할 연결
	void InitializeAbilitySystem(UAbilitySystemComponent* InASC, AActor* InOwnerActor);

	// Avatar 연결 해제 및 외부 소유 ASC 참조 정리
	void UninitializeAbilitySystem();

	// 몬스터는 직접 생성하고 플레이어는 PlayerState에서 연결하는 공통 ASC 참조
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UTBAbilitySystemComponent> AbilitySystemComponent;
	
	// 자식 클래스에서 지정하는 고정 캐릭터 타입
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "GAS|Type")
	FGameplayTag CharacterTypeTag;
	
	// 캐릭터 BP에서 지정할 혈액 피격 연출 태그
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|GameplayCue",
		meta = (Categories = "GameplayCue"))
	FGameplayTag BloodImpactCueTag;
};
