#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "TBAttributeSetBase.generated.h"

// Attribute 식별자와 값 조회·설정·초기화 접근자 생성
#define TB_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

// 보유 혈액과 탄창에 충전된 혈액의 수치 및 복제 관리
UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBAttributeSetBase : public UAttributeSet
{
	GENERATED_BODY()

public:
	UTBAttributeSetBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Modifier가 반영된 CurrentValue의 변경 전 범위 제한
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	// BaseValue의 변경 전 범위 제한
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	// GameplayEffect 실행 이후 자원 및 최대량의 일관성 보정
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	// 보유 혈액 ----------------------

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Blood, Category = "Blood")
	FGameplayAttributeData Blood;
	TB_ATTRIBUTE_ACCESSORS(ThisClass, Blood)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxBlood, Category = "Blood")
	FGameplayAttributeData MaxBlood;
	TB_ATTRIBUTE_ACCESSORS(ThisClass, MaxBlood)

	// 탄창 혈액 ----------------------

	// 보유 혈액과 별도로 관리하는 발사용 충전 자원
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_LoadedBlood, Category = "Blood|Loaded")
	FGameplayAttributeData LoadedBlood;
	TB_ATTRIBUTE_ACCESSORS(ThisClass, LoadedBlood)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxLoadedBlood, Category = "Blood|Loaded")
	FGameplayAttributeData MaxLoadedBlood;
	TB_ATTRIBUTE_ACCESSORS(ThisClass, MaxLoadedBlood)
	
	// 마지막 재장전 완료 당시 실제 장전량
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AmmoAtLastReload, Category = "Blood|Loaded")
	FGameplayAttributeData AmmoAtLastReload;
	TB_ATTRIBUTE_ACCESSORS(ThisClass, AmmoAtLastReload)

protected:
	// Attribute 복제 알림 ----------------------

	UFUNCTION()
	void OnRep_Blood(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxBlood(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_LoadedBlood(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxLoadedBlood(const FGameplayAttributeData& OldValue);
	
	UFUNCTION()
	void OnRep_AmmoAtLastReload(const FGameplayAttributeData& OldValue);

private:
	// Attribute별 허용 범위 제한
	virtual void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;
};
