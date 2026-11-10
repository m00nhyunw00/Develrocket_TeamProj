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

bool UTBHealComponent::ApplyHeal(float Amount)
{
	AActor* Owner = GetOwner();

	// 서버 + 유효한 값 + GE 설정 확인
	if (!Owner || !Owner->HasAuthority() || !HealEffectClass
		|| !FMath::IsFinite(Amount) || Amount <= 0.0f)
	{
		return false;
	}

	// ASC는 호출할 때 가져오기 (플레이어 ASC는 PlayerState에 있어서 BeginPlay엔 아직 없을 수 있음)
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

	// 회복량 싣기
	Spec.Data->SetSetByCallerMagnitude(TBGameplayTags::Data_Heal, Amount);

	return ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()).WasSuccessfullyApplied();
}