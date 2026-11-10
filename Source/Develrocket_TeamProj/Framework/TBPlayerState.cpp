#include "Framework/TBPlayerState.h"

#include "AbilitySystem/TBAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/TBAttributeSetBase.h"
#include "GameplayEffect.h"
#include "TBGameplayTags.h"
#include "AbilitySystem/Attributes/TBPlayerAttributeSet.h"
#include "Character/Player/TBPlayer.h"

ATBPlayerState::ATBPlayerState()
{
	// Pawn 교체 이후에도 유지할 커스텀 ASC
	AbilitySystemComponent = CreateDefaultSubobject<UTBAbilitySystemComponent>(TEXT("AbilitySystemComponent"));

	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// 자원 AttributeSet 생성 및 등록
	PlayerAttributeSet = CreateDefaultSubobject<UTBPlayerAttributeSet>(TEXT("BloodAttributeSet"));

	AbilitySystemComponent->AddAttributeSetSubobject(PlayerAttributeSet.Get());
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
	
	// 초기 체력 및 탄약 설정
	const FActiveGameplayEffectHandle AppliedHandle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

	// 초기 설정 적용에 실패하면 종료
	if (!AppliedHandle.WasSuccessfullyApplied())
	{
		return;
	}

	// 설정된 현재 탄약을 오른쪽 표시용 기준값으로 저장
	AbilitySystemComponent->SetNumericAttributeBase(UTBPlayerAttributeSet::GetAmmoAtLastReloadAttribute(),PlayerAttributeSet->GetLoadedBlood());

	// BP에서 지정한 초기 어빌리티 부여
	AbilitySystemComponent->AddCharacterAbilities(StartupAbilities);

	bGameplayDataInitialized = true;
}

void ATBPlayerState::InitializeDeathEvents()
{
	if (bDeathEventsBound || !IsValid(AbilitySystemComponent.Get()))
	{
		return;
	}

	bDeathEventsBound = true;

	// 서버와 클라이언트에서 사망 상태 변화 감지
	AbilitySystemComponent->RegisterGameplayTagEvent(TBGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleDeadTagChanged);

	// 혈액 감소에 따른 사망 판정은 서버에서만 수행
	if (HasAuthority())
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UTBAttributeSetBase::GetBloodAttribute()).AddUObject(this, &ThisClass::HandleBloodChanged);
	}
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

void ATBPlayerState::HandleBloodChanged(const FOnAttributeChangeData& Data)
{
	if (!HasAuthority() || !bGameplayDataInitialized || Data.NewValue > 0.0f)
	{
		return;
	}

	ATBPlayer* Player = Cast<ATBPlayer>(AbilitySystemComponent->GetAvatarActor());

	if (IsValid(Player)
		&& Player->GetAbilitySystemComponent() == AbilitySystemComponent.Get())
	{
		Player->Die();
	}
}

void ATBPlayerState::HandleDeadTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	ATBPlayer* Player = Cast<ATBPlayer>(AbilitySystemComponent->GetAvatarActor());

	if (IsValid(Player) && Player->GetAbilitySystemComponent() == AbilitySystemComponent.Get())
	{
		Player->HandleDeadTagChanged(Tag, NewCount);
	}
}
