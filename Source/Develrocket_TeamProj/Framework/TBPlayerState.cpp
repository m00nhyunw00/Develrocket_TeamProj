#include "Framework/TBPlayerState.h"

#include "AbilitySystem/TBAbilitySystemComponent.h"
#include "AbilitySystem/Abilities/Player/TBGameplayAbility_BloodPulse.h"
#include "AbilitySystem/Attributes/TBAttributeSetBase.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "TBGameplayTags.h"
#include "AbilitySystem/Attributes/TBPlayerAttributeSet.h"
#include "Character/Player/TBPlayer.h"
#include "Data/TBCharacterClassData.h"

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

void ATBPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATBPlayerState, MatchStats);
	DOREPLIFETIME(ATBPlayerState, SelectedClassData);
}

void ATBPlayerState::AddMatchKill(int32 Points)
{
	if (!HasAuthority())
	{
		return;
	}

	++MatchStats.Kills;
	MatchStats.MatchScore += Points;
	ForceNetUpdate();
}

void ATBPlayerState::AddMatchDeath()
{
	if (!HasAuthority())
	{
		return;
	}

	++MatchStats.Deaths;
	ForceNetUpdate();
}

void ATBPlayerState::AddMatchScore(int32 Points)
{
	if (!HasAuthority() || Points <= 0)
	{
		return;
	}

	MatchStats.MatchScore += Points;
	ForceNetUpdate();
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
	const bool bHasSkill1 = AbilitySystemComponent->GetActivatableAbilities().ContainsByPredicate(
		[](const FGameplayAbilitySpec& Spec)
		{
			return Spec.Ability && Spec.Ability->GetAssetTags().HasTagExact(TBGameplayTags::Ability_Combat_Skill1);
		});
	if (!bHasSkill1)
	{
		AbilitySystemComponent->AddCharacterAbilities({UTBGameplayAbility_BloodPulse::StaticClass()});
	}

	bGameplayDataInitialized = true;
}

bool ATBPlayerState::ResetForRespawn(FActiveGameplayEffectHandle DeathEffect)
{
	if (!HasAuthority() || !bGameplayDataInitialized || !IsValid(AbilitySystemComponent)
		|| !IsValid(PlayerAttributeSet) || PlayerAttributeSet->GetMaxBlood() <= 0.0f)
	{
		return false;
	}

	AbilitySystemComponent->CancelAllAbilities();
	if (DeathEffect.IsValid())
	{
		AbilitySystemComponent->RemoveActiveGameplayEffect(DeathEffect, -1);
	}

	if (AbilitySystemComponent->HasMatchingGameplayTag(TBGameplayTags::State_Dead))
	{
		return false;
	}

	AbilitySystemComponent->SetNumericAttributeBase(UTBAttributeSetBase::GetBloodAttribute(), PlayerAttributeSet->GetMaxBlood());
	AbilitySystemComponent->SetNumericAttributeBase(UTBPlayerAttributeSet::GetLoadedBloodAttribute(), PlayerAttributeSet->GetMaxLoadedBlood());
	AbilitySystemComponent->SetNumericAttributeBase(UTBPlayerAttributeSet::GetAmmoAtLastReloadAttribute(), PlayerAttributeSet->GetLoadedBlood());
	ForceNetUpdate();
	return true;
}

void ATBPlayerState::InitializeDeathEvents()
{
	if (!HasAuthority() || bDeathEventsBound || !IsValid(AbilitySystemComponent.Get()))
	{
		return;
	}

	bDeathEventsBound = true;

	// 사망 판정은 서버 ASC에서 처리하고 사망 상태는 Pawn이 복제합니다.
	AbilitySystemComponent->RegisterGameplayTagEvent(TBGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleDeadTagChanged);

	// 혈액 감소에 따른 사망 판정은 서버에서만 수행
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UTBAttributeSetBase::GetBloodAttribute()).AddUObject(this, &ThisClass::HandleBloodChanged);
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

bool ATBPlayerState::GetAttackDamage(const FGameplayTag& AttackId, float& OutDamage) const
{
	OutDamage = 0.0f;

	// 1단계 이전 중: DA가 지정되지 않은 BP는 기존 필드 값 유지
	const UTBCharacterClassData* Data = GetClassData();

	if (AttackId == TBGameplayTags::Ability_Combat_PrimaryAttack)
	{
		OutDamage = Data ? Data->PrimaryAttackDamage : PrimaryAttackDamage;
	}
	else if (AttackId == TBGameplayTags::Ability_Combat_SecondaryAction)
	{
		OutDamage = Data ? Data->SecondaryAttackDamage : SecondaryAttackDamage;
	}
	else if (AttackId == TBGameplayTags::Ability_Combat_Mobility)
	{
		OutDamage = Data ? Data->MobilityAttackDamage : MobilityAttackDamage;
	}
	else if (AttackId == TBGameplayTags::Ability_Combat_Melee)
	{
		OutDamage = Data ? Data->MeleeAttackDamage : MeleeAttackDamage;
	}
	else if (AttackId == TBGameplayTags::Ability_Combat_Skill1)
	{
		OutDamage = Data ? Data->Skill1Damage : Skill1Damage;
	}
	else if (AttackId == TBGameplayTags::Ability_Combat_Skill2)
	{
		OutDamage = Data ? Data->Skill2Damage : Skill2Damage;
	}
	else
	{
		return false;
	}

	return FMath::IsFinite(OutDamage) && OutDamage > 0.0f;
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
		const ATBPlayer* Attacker = Data.GEModData
			? Cast<ATBPlayer>(Data.GEModData->EffectSpec.GetContext().GetSourceObject())
			: nullptr;
		Player->Die(IsValid(Attacker) ? Attacker->GetPlayerState<ATBPlayerState>() : nullptr);
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

const UTBCharacterClassData* ATBPlayerState::GetClassData() const
{
	return SelectedClassData ? SelectedClassData.Get() : DefaultClassData.Get();
}

void ATBPlayerState::SetSelectedClassData(const UTBCharacterClassData* NewClassData)
{
	if (!HasAuthority() || !NewClassData || SelectedClassData == NewClassData)
	{
		return;
	}

	SelectedClassData = NewClassData;
	ForceNetUpdate();

	// 서버(호스트)는 OnRep이 호출되지 않으므로 직접 알림
	OnClassDataChangedEvent.Broadcast();
}

void ATBPlayerState::OnRep_SelectedClassData()
{
	OnClassDataChangedEvent.Broadcast();
}

// 직업 데이터 기반 조회 ----------------------
// 1단계 이전 중: DA가 지정되지 않은 BP는 기존 필드 값 유지

float ATBPlayerState::GetPrimaryAttackDamage() const
{
	const UTBCharacterClassData* Data = GetClassData();
	return Data ? Data->PrimaryAttackDamage : PrimaryAttackDamage;
}

float ATBPlayerState::GetMeleeAttackDamage() const
{
	const UTBCharacterClassData* Data = GetClassData();
	return Data ? Data->MeleeAttackDamage : MeleeAttackDamage;
}

float ATBPlayerState::GetPrimaryAttackRange() const
{
	const UTBCharacterClassData* Data = GetClassData();
	return Data ? Data->PrimaryAttackRange : PrimaryAttackRange;
}

float ATBPlayerState::GetPrimaryAttackInterval() const
{
	const UTBCharacterClassData* Data = GetClassData();
	return Data ? Data->PrimaryAttackInterval : PrimaryAttackInterval;
}
