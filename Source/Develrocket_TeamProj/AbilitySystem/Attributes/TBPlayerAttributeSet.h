#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Attributes/TBAttributeSetBase.h"
#include "TBPlayerAttributeSet.generated.h"

/**
 * 
 */
UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBPlayerAttributeSet : public UTBAttributeSetBase
{
	GENERATED_BODY()
	
public:
	UTBPlayerAttributeSet();
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	
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
	
	// 달리기와 강화가 합산하는 이동 속도 증가 비율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MoveSpeedBonus, Category = "Movement")
	FGameplayAttributeData MoveSpeedBonus;
	TB_ATTRIBUTE_ACCESSORS(ThisClass, MoveSpeedBonus)
	
protected:
	virtual void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const override;
	
	UFUNCTION()
	void OnRep_LoadedBlood(const FGameplayAttributeData& OldValue);
	
	UFUNCTION()
	void OnRep_MaxLoadedBlood(const FGameplayAttributeData& OldValue);
	
	UFUNCTION()
	void OnRep_AmmoAtLastReload(const FGameplayAttributeData& OldValue);
	
	UFUNCTION()
	void OnRep_MoveSpeedBonus(const FGameplayAttributeData& OldValue);
};
