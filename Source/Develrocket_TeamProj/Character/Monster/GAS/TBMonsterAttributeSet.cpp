#include "Character/Monster/GAS/TBMonsterAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "TBGameplayTags.h"

UTBMonsterAttributeSet::UTBMonsterAttributeSet()
{
	InitMaxBlood(100.0f);
	InitBlood(100.0f);
	InitIncomingDamage(0.0f);
	InitMaxStagger(50.0f);
	InitStagger(0.0f);
}

void UTBMonsterAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 보스 체력 표시를 위한 모든 관련 클라이언트 복제
	RESET_REPLIFETIME_CONDITION(UTBAttributeSetBase, Blood, COND_None);
	RESET_REPLIFETIME_CONDITION(UTBAttributeSetBase, MaxBlood, COND_None);
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

void UTBMonsterAttributeSet::AddStagger(float Damage)
{
	if (!GetOwningActor() || !GetOwningActor()->HasAuthority() || HasDied()
		|| !FMath::IsFinite(Damage) || Damage <= 0.0f
		|| !FMath::IsFinite(GetStagger()) || !FMath::IsFinite(GetMaxStagger()) || GetMaxStagger() <= 0.0f)
	{
		return;
	}

	const float PreviousStagger = GetStagger();
	const float MaxStaggerValue = GetMaxStagger();
	const float NewStagger = FMath::Clamp(PreviousStagger + Damage, 0.0f, MaxStaggerValue);
	SetStagger(NewStagger);

	if (PreviousStagger < MaxStaggerValue && GetStagger() >= MaxStaggerValue)
	{
		OnStaggerFull.Broadcast();
	}
}

void UTBMonsterAttributeSet::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::ClampAttribute(Attribute, NewValue);
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
			SetBlood(FMath::Clamp(GetBlood() - Damage, 0.0f, GetMaxBlood()));
		}
	}
	else if (Attribute == GetBloodAttribute() || Attribute == GetMaxBloodAttribute())
	{
		SetMaxBlood(FMath::Max(1.0f, GetMaxBlood()));
		SetBlood(HasDied() ? 0.0f : FMath::Clamp(GetBlood(), 0.0f, GetMaxBlood()));
	}

	if (GetBlood() <= 0.0f && GetOwningActor() && GetOwningActor()->HasAuthority() && !HasDied())
	{
		OnOutOfBlood.Broadcast();
	}
}

