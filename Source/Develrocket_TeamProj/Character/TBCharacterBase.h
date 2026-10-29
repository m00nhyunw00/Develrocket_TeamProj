#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "TBCharacterBase.generated.h"

class UAbilitySystemComponent;

UCLASS(Abstract)
class DEVELROCKET_TEAMPROJ_API ATBCharacterBase
	: public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ATBCharacterBase(const FObjectInitializer& ObjectInitializer);

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

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

	// 현재 캐릭터의 ASC 연결과 캐시 해제
	void UninitializeAbilitySystem();

	// 외부에서 전달받아 현재 캐릭터에 연결한 ASC 참조
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> CachedASC;
};
