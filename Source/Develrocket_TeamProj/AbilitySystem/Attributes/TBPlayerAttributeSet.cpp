#include "AbilitySystem/Attributes/TBPlayerAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UTBPlayerAttributeSet::UTBPlayerAttributeSet()
{
	InitMaxLoadedBlood(20.0f);
	InitLoadedBlood(20.0f);
	InitAmmoAtLastReload(20.0f);
}

void UTBPlayerAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UTBPlayerAttributeSet, LoadedBlood,COND_OwnerOnly, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(UTBPlayerAttributeSet, MaxLoadedBlood, COND_OwnerOnly, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(UTBPlayerAttributeSet, AmmoAtLastReload, COND_OwnerOnly, REPNOTIFY_Always);
}

void UTBPlayerAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	const FGameplayAttribute& Attribute = Data.EvaluatedData.Attribute;

	// 탄창 용량 감소 시 현재 장전량도 제한
	if (Attribute == GetLoadedBloodAttribute() || Attribute == GetMaxLoadedBloodAttribute())
	{
		SetMaxLoadedBlood(FMath::Max(0.0f, GetMaxLoadedBlood()));

		SetLoadedBlood(FMath::Clamp(GetLoadedBlood(), 0.0f, GetMaxLoadedBlood()));
	}
}

void UTBPlayerAttributeSet::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::ClampAttribute(Attribute, NewValue);

	// 플레이어 전용 탄약 속성 제한
	if (Attribute == GetLoadedBloodAttribute())
	{
		NewValue = FMath::Clamp(NewValue,0.0f, FMath::Max(0.0f, GetMaxLoadedBlood()));
	}
	else if (Attribute == GetMaxLoadedBloodAttribute())
	{
		NewValue = FMath::Max(0.0f, NewValue);
	}
}

void UTBPlayerAttributeSet::OnRep_LoadedBlood(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UTBPlayerAttributeSet, LoadedBlood, OldValue);
}

void UTBPlayerAttributeSet::OnRep_MaxLoadedBlood(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UTBPlayerAttributeSet, MaxLoadedBlood, OldValue);
}

void UTBPlayerAttributeSet::OnRep_AmmoAtLastReload(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UTBPlayerAttributeSet, AmmoAtLastReload, OldValue);
}