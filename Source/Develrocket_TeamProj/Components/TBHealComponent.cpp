#include "Components/TBHealComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"
#include "TBGameplayTags.h"

UTBHealComponent::UTBHealComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	// RPC 없음: 서버에서 GE를 적용하면 Blood는 GAS가 알아서 복제
}

bool UTBHealComponent::ApplyHeal()
{
	AActor* Owner = GetOwner();

	// 서버 + GE 설정 확인 (Amount 검사 제거)
	if (!Owner || !Owner->HasAuthority() || !HealEffectClass)
	{
		return false;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner);
	if (!ASC || ASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead))
	{
		return false;
	}

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(Owner);

	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(HealEffectClass, 1.0f, Context);
	if (!Spec.IsValid())
	{
		return false;
	}

	// SetSetByCallerMagnitude 줄 삭제: 회복량은 GE에 고정

	return ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()).WasSuccessfullyApplied();
}