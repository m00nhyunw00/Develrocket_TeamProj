#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "AbilitySystem/Attributes/TBAttributeSetBase.h"
#include "TBMonsterAttributeSet.generated.h"

DECLARE_MULTICAST_DELEGATE(FTBMonsterOutOfBlood);
DECLARE_MULTICAST_DELEGATE(FTBMonsterStaggerFull);

// 몬스터 체력 복제 및 서버 피해 계산
UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBMonsterAttributeSet : public UTBAttributeSetBase
{
	GENERATED_BODY()

public:
	UTBMonsterAttributeSet();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	UPROPERTY(BlueprintReadOnly, Category = "Monster|Damage")
	FGameplayAttributeData IncomingDamage;
	TB_ATTRIBUTE_ACCESSORS(ThisClass, IncomingDamage)

	UPROPERTY(BlueprintReadOnly, Category = "Monster|Damage")
	FGameplayAttributeData Stagger;
	TB_ATTRIBUTE_ACCESSORS(ThisClass, Stagger)
	
	UPROPERTY(BlueprintReadOnly, Category = "Monster|Damage")
	FGameplayAttributeData MaxStagger;
	TB_ATTRIBUTE_ACCESSORS(ThisClass, MaxStagger)
	
	FTBMonsterOutOfBlood OnOutOfBlood;
	FTBMonsterStaggerFull OnStaggerFull;

	// 서버에서 그로기 수치 누적 및 최대치 도달 알림
	void AddStagger(float Damage);

private:
	virtual void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;
	bool HasDied() const;
};
