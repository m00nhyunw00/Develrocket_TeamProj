#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "AbilitySystem/Attributes/TBAttributeSetBase.h"
#include "TBMonsterAttributeSet.generated.h"

#define TB_MONSTER_ATTRIBUTE_ACCESSORS(PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UTBMonsterAttributeSet, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

DECLARE_MULTICAST_DELEGATE(FTBMonsterOutOfHealth);
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
	TB_MONSTER_ATTRIBUTE_ACCESSORS(IncomingDamage)

	UPROPERTY(BlueprintReadOnly, Category = "Monster|Damage")
	FGameplayAttributeData Stagger;
	TB_MONSTER_ATTRIBUTE_ACCESSORS(Stagger)
	
	UPROPERTY(BlueprintReadOnly, Category = "Monster|Damage")
	FGameplayAttributeData MaxStagger;
	TB_MONSTER_ATTRIBUTE_ACCESSORS(MaxStagger)
	
	FTBMonsterOutOfHealth OnOutOfHealth;
	FTBMonsterStaggerFull OnStaggerFull;

	// 서버에서 그로기 수치 누적 및 최대치 도달 알림
	UFUNCTION(BlueprintCallable) void AddStagger(float Damage);
	// 기존 C++ 호출을 공통 Blood 속성으로 연결
	float GetHealth() const { return GetBlood(); }
	float GetMaxHealth() const { return GetMaxBlood(); }
	static FGameplayAttribute GetHealthAttribute() { return GetBloodAttribute(); }
	static FGameplayAttribute GetMaxHealthAttribute() { return GetMaxBloodAttribute(); }

private:
	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;
	bool HasDied() const;
};

#undef TB_MONSTER_ATTRIBUTE_ACCESSORS
