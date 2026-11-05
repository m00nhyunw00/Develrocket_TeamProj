#include "AbilitySystem/Attributes/TBAttributeSetBase.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UTBAttributeSetBase::UTBAttributeSetBase()
{
	InitMaxBlood(100.0f);
	InitBlood(0.0f);

	InitMaxLoadedBlood(20.0f);
	InitLoadedBlood(20.0f);
	InitAmmoAtLastReload(20.0f);
}

void UTBAttributeSetBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 자원 수치는 소유 플레이어에게만 복제
	// 수신 값이 로컬 값과 같아도 RepNotify를 호출하여 GAS 예측 보정 반영
	DOREPLIFETIME_CONDITION_NOTIFY(UTBAttributeSetBase, Blood, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTBAttributeSetBase, MaxBlood, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTBAttributeSetBase, LoadedBlood, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTBAttributeSetBase, MaxLoadedBlood, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTBAttributeSetBase, AmmoAtLastReload, COND_OwnerOnly, REPNOTIFY_Always);
}

void UTBAttributeSetBase::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	ClampAttribute(Attribute, NewValue);
}

void UTBAttributeSetBase::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	ClampAttribute(Attribute, NewValue);
}

void UTBAttributeSetBase::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetBloodAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, FMath::Max(0.0f, GetMaxBlood()));
	}
	else if (Attribute == GetMaxBloodAttribute())
	{
		NewValue = FMath::Max(0.0f, NewValue);
	}
	else if (Attribute == GetLoadedBloodAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, FMath::Max(0.0f, GetMaxLoadedBlood()));
	}
	else if (Attribute == GetMaxLoadedBloodAttribute())
	{
		NewValue = FMath::Max(0.0f, NewValue);
	}
}

void UTBAttributeSetBase::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	const FGameplayAttribute& Attribute = Data.EvaluatedData.Attribute;

	// 최대 보유량 감소 시 현재 혈액도 새 상한에 맞추어 제한
	if (Attribute == GetBloodAttribute() || Attribute == GetMaxBloodAttribute())
	{
		SetMaxBlood(FMath::Max(0.0f, GetMaxBlood()));
		SetBlood(FMath::Clamp(GetBlood(), 0.0f, GetMaxBlood()));
	}

	// 탄창 용량 감소 시 충전된 혈액도 새 상한에 맞추어 제한
	if (Attribute == GetLoadedBloodAttribute() || Attribute == GetMaxLoadedBloodAttribute())
	{
		SetMaxLoadedBlood(FMath::Max(0.0f, GetMaxLoadedBlood()));
		SetLoadedBlood(FMath::Clamp(GetLoadedBlood(), 0.0f, GetMaxLoadedBlood()));
	}
}

// 복제된 Attribute 값을 ASC의 집계 및 예측 보정에 전달
void UTBAttributeSetBase::OnRep_Blood(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UTBAttributeSetBase, Blood, OldValue);
}

void UTBAttributeSetBase::OnRep_MaxBlood(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UTBAttributeSetBase, MaxBlood, OldValue);
}

void UTBAttributeSetBase::OnRep_LoadedBlood(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UTBAttributeSetBase, LoadedBlood, OldValue);
}

void UTBAttributeSetBase::OnRep_MaxLoadedBlood(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UTBAttributeSetBase, MaxLoadedBlood, OldValue);
}

void UTBAttributeSetBase::OnRep_AmmoAtLastReload(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UTBAttributeSetBase, AmmoAtLastReload, OldValue);
}
