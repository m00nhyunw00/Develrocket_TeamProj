#include "AbilitySystem/Attributes/TBBloodAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UTBBloodAttributeSet::UTBBloodAttributeSet()
{
	InitMaxBlood(100.0f);
	InitBlood(0.0f);

	InitMaxLoadedBlood(20.0f);
	InitLoadedBlood(20.0f);
}

void UTBBloodAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 자원 수치는 소유 플레이어에게만 복제
	// 수신 값이 로컬 값과 같아도 RepNotify를 호출하여 GAS 예측 보정 반영
	DOREPLIFETIME_CONDITION_NOTIFY(UTBBloodAttributeSet, Blood, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTBBloodAttributeSet, MaxBlood, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTBBloodAttributeSet, LoadedBlood, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTBBloodAttributeSet, MaxLoadedBlood, COND_OwnerOnly, REPNOTIFY_Always);
}

void UTBBloodAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	ClampAttribute(Attribute, NewValue);
}

void UTBBloodAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	ClampAttribute(Attribute, NewValue);
}

void UTBBloodAttributeSet::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
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

void UTBBloodAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
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
void UTBBloodAttributeSet::OnRep_Blood(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UTBBloodAttributeSet, Blood, OldValue);
}

void UTBBloodAttributeSet::OnRep_MaxBlood(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UTBBloodAttributeSet, MaxBlood, OldValue);
}

void UTBBloodAttributeSet::OnRep_LoadedBlood(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UTBBloodAttributeSet, LoadedBlood, OldValue);
}

void UTBBloodAttributeSet::OnRep_MaxLoadedBlood(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UTBBloodAttributeSet, MaxLoadedBlood, OldValue);
}
