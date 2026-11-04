#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "TBMonsterAttributeSet.generated.h"

#define TB_MONSTER_ATTRIBUTE_ACCESSORS(PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UTBMonsterAttributeSet, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

DECLARE_MULTICAST_DELEGATE(FTBMonsterOutOfHealth);

// 몬스터 체력 복제 및 서버 피해 계산
UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBMonsterAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UTBMonsterAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Monster|Health")
	FGameplayAttributeData Health;
	TB_MONSTER_ATTRIBUTE_ACCESSORS(Health)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Monster|Health")
	FGameplayAttributeData MaxHealth;
	TB_MONSTER_ATTRIBUTE_ACCESSORS(MaxHealth)

	// 피해 GE가 전달하는 일회성 수치; 실행 후 초기화 및 복제 제외
	UPROPERTY(BlueprintReadOnly, Category = "Monster|Damage")
	FGameplayAttributeData IncomingDamage;
	TB_MONSTER_ATTRIBUTE_ACCESSORS(IncomingDamage)

	FTBMonsterOutOfHealth OnOutOfHealth;

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

private:
	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;
	bool HasDied() const;
};

#undef TB_MONSTER_ATTRIBUTE_ACCESSORS
