#include "Character/Monster/GAS/TBMonsterAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "TBGameplayTags.h"

UTBMonsterAttributeSet::UTBMonsterAttributeSet()
{
	InitMaxHealth(100.0f);
	InitHealth(100.0f);
	InitIncomingDamage(0.0f);
}

void UTBMonsterAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 보스 체력 표시를 위한 모든 관련 클라이언트 복제
	DOREPLIFETIME_CONDITION_NOTIFY(UTBMonsterAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTBMonsterAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
}

void UTBMonsterAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UTBMonsterAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

bool UTBMonsterAttributeSet::HasDied() const
{
	const UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	return ASC && ASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead);
}

void UTBMonsterAttributeSet::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (!FMath::IsFinite(NewValue))
	{
		NewValue = Attribute == GetMaxHealthAttribute() ? 1.0f : 0.0f;
	}

	if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(1.0f, NewValue);
	}
	else if (Attribute == GetHealthAttribute())
	{
		NewValue = HasDied() ? 0.0f : FMath::Clamp(NewValue, 0.0f, FMath::Max(1.0f, GetMaxHealth()));
	}
}

void UTBMonsterAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	const FGameplayAttribute& Attribute = Data.EvaluatedData.Attribute;
	if (Attribute == GetIncomingDamageAttribute())
	{
		const float Damage = GetIncomingDamage();
		SetIncomingDamage(0.0f);
		if (FMath::IsFinite(Damage) && Damage > 0.0f && !HasDied())
		{
			SetHealth(FMath::Clamp(GetHealth() - Damage, 0.0f, GetMaxHealth()));
		}
	}
	else if (Attribute == GetHealthAttribute() || Attribute == GetMaxHealthAttribute())
	{
		SetMaxHealth(FMath::Max(1.0f, GetMaxHealth()));
		SetHealth(HasDied() ? 0.0f : FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
	}

	if (GetHealth() <= 0.0f && GetOwningActor() && GetOwningActor()->HasAuthority() && !HasDied())
	{
		OnOutOfHealth.Broadcast();
	}
}

void UTBMonsterAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UTBMonsterAttributeSet, Health, OldValue);
}

void UTBMonsterAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UTBMonsterAttributeSet, MaxHealth, OldValue);
}
