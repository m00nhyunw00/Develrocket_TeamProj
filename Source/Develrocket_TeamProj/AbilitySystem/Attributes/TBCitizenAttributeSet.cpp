#include "AbilitySystem/Attributes/TBCitizenAttributeSet.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "TBGameplayTags.h"

UTBCitizenAttributeSet::UTBCitizenAttributeSet()
{
	InitMaxBlood(50.f);
	InitBlood(50.f);
}

void UTBCitizenAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    // 다른 클라이언트가 머리 위 HP 바를 갱신할 수 있도록 Blood를 복제한다.
    RESET_REPLIFETIME_CONDITION(UTBAttributeSetBase, Blood, COND_None);
    RESET_REPLIFETIME_CONDITION(UTBAttributeSetBase, MaxBlood, COND_None);
}

bool UTBCitizenAttributeSet::PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data)
{
	if (!Super::PreGameplayEffectExecute(Data))
	{
		return false;
	}

	// 혈액 감소(피격·출혈)만 검사, 회복은 통과
	if (Data.EvaluatedData.Attribute == GetBloodAttribute() && Data.EvaluatedData.Magnitude < 0.f)
	{
		const UAbilitySystemComponent& TargetASC = Data.Target;
		if (TargetASC.HasMatchingGameplayTag(TBGameplayTags::State_Invulnerable)
			|| TargetASC.HasMatchingGameplayTag(TBGameplayTags::State_Dead))
		{
			return false;   // 이번 실행만 취소 (주기 GE는 이번 틱만 건너뜀)
		}
	}
	return true;
}

void UTBCitizenAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);   // 부모가 Blood를 0~MaxBlood로 보정

	if (Data.EvaluatedData.Attribute == GetBloodAttribute() && GetBlood() <= 0.f
		&& !Data.Target.HasMatchingGameplayTag(TBGameplayTags::State_Dead))
	{
		OnOutOfBlood.Broadcast();
	}
}