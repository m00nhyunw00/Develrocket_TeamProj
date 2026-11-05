#include "Framework/TBPlayerState.h"

#include "AbilitySystem/TBAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/TBAttributeSetBase.h"
#include "GameplayEffect.h"

ATBPlayerState::ATBPlayerState()
{
	// Pawn 교체 이후에도 유지할 커스텀 ASC
	AbilitySystemComponent = CreateDefaultSubobject<UTBAbilitySystemComponent>(TEXT("AbilitySystemComponent"));

	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// 자원 AttributeSet 생성 및 등록
	BloodAttributeSet = CreateDefaultSubobject<UTBAttributeSetBase>(TEXT("BloodAttributeSet"));
	AbilitySystemComponent->AddAttributeSetSubobject(BloodAttributeSet.Get());
}

UAbilitySystemComponent* ATBPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent.Get();
}

void ATBPlayerState::InitializeGameplayData()
{
	if (!HasAuthority() || bGameplayDataInitialized || !AbilitySystemComponent)
	{
		return;
	}

	if (!InitialBloodEffect)
	{
		UE_LOG(LogTemp, Warning, TEXT("InitialBloodEffect가 설정되지 않았습니다."));
		return;
	}

	FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
	Context.AddSourceObject(this);

	const FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(InitialBloodEffect, 1.0f, Context);

	if (!Spec.IsValid())
	{
		return;
	}

	// 초기 혈액 및 탄창 설정
	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

	// 기본 Ability 부여 및 핸들 기록
	AbilitySystemComponent->AddCharacterAbilities(StartupAbilities);

	bGameplayDataInitialized = true;
}

void ATBPlayerState::ApplyBloodRecovery()
{
	// 회복 승인 및 적용은 서버에서 수행
	if (!HasAuthority() || !AbilitySystemComponent || !BloodRecoveryEffect)
	{
		return;
	}

	FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
	Context.AddSourceObject(this);

	const FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(BloodRecoveryEffect, 1.0f, Context);

	if (Spec.IsValid())
	{
		AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}