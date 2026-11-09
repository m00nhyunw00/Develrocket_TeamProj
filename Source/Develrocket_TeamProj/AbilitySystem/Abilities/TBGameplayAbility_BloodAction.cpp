
#include "AbilitySystem/Abilities/TBGameplayAbility_BloodAction.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/TBAttributeSetBase.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/Player/TBPlayer.h"
#include "Components/SkeletalMeshComponent.h"
#include "TBGameplayTags.h"
#include "AbilitySystem/TBAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/TBPlayerAttributeSet.h"

UTBGameplayAbility_BloodAction::UTBGameplayAbility_BloodAction()
{
	// ASC마다 별도의 GameplayAbility 인스턴스 사용
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// 소유 클라이언트 예측 실행 및 서버 검증
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// 클라이언트의 종료 요청으로 서버 행동 시간이 단축되지 않도록 제한
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnlyTermination;

	bRetriggerInstancedAbility = false;

	// 실행 중 전투 상태 부여 및 다른 전투 행동 차단
	ActivationOwnedTags.AddTag(TBGameplayTags::State_Combat);
	
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Combat);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Stunned);
}

UTBGameplayAbility_PrimaryAttack::UTBGameplayAbility_PrimaryAttack()
{
	SetAssetTags(FGameplayTagContainer(TBGameplayTags::Ability_Combat_PrimaryAttack));

	// 공격 시작 시 충전 혈액 비용 적용
	bCommitOnActivate = true;
}

void UTBGameplayAbility_PrimaryAttack::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// 기존 비용 처리 및 몽타주 재생
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 비용 부족 또는 실행 중 종료 여부 확인
	if (!IsActive() || !ActorInfo)
	{
		return;
	}

	ATBPlayer* Player = Cast<ATBPlayer>(ActorInfo->AvatarActor.Get());

	if (!IsValid(Player))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 서버 적중 판정 및 피해 적용
	if (ActorInfo->IsNetAuthority())
	{
		Player->PerformPrimaryAttackTrace();
	}

	// 소유 플레이어 화면의 발사 알림
	if (Player->IsLocallyControlled())
	{
		if (UTBAbilitySystemComponent* TBASC = Cast<UTBAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()))
		{
			TBASC->OnPrimaryShotFired.Broadcast();
		}
	}
}

UTBGameplayAbility_Reload::UTBGameplayAbility_Reload()
{
	SetAssetTags(FGameplayTagContainer(TBGameplayTags::Ability_Combat_Reload));

	// 재장전 완료 시 서버에서 자원 전환
	bCommitOnActivate = false;
}

bool UTBGameplayAbility_Reload::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const UTBPlayerAttributeSet* Attributes = ASC ? ASC->GetSet<UTBPlayerAttributeSet>() : nullptr;


	if (!Attributes || !GetCostGameplayEffect())
	{
		return false;
	}

	// 현재 탄창에서 추가로 채울 수 있는 양 계산
	const float MissingAmmo = Attributes->GetMaxLoadedBlood() - Attributes->GetLoadedBlood();

	// 탄창에 빈 공간이 있고 보유 혈액이 있을 때 재장전 허용
	return MissingAmmo > KINDA_SMALL_NUMBER && Attributes->GetBlood() > KINDA_SMALL_NUMBER;
}

void UTBGameplayAbility_Reload::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	// 재장전 자원 변경은 서버에서만 확정
	if (!ActorInfo || !ActorInfo->IsNetAuthority())
	{
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	const UTBPlayerAttributeSet* Attributes = ASC ? ASC->GetSet<UTBPlayerAttributeSet>() : nullptr;
	const UGameplayEffect* ReloadEffect = GetCostGameplayEffect();

	if (!Attributes || !ReloadEffect)
	{
		return;
	}

	// 재장전 완료 시점의 탄창 부족량 계산
	const float MissingAmmo = Attributes->GetMaxLoadedBlood() - Attributes->GetLoadedBlood();

	// 보유 혈액을 초과하지 않는 범위에서 실제 충전량 결정
	const float ReloadAmount = FMath::Min(MissingAmmo, Attributes->GetBlood());
	if (ReloadAmount <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// 이번 재장전에 사용할 GE Spec 생성
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(ReloadEffect->GetClass(), GetAbilityLevel(Handle, ActorInfo), MakeEffectContext(Handle, ActorInfo));

	if (!Spec.IsValid())
	{
		return;
	}

	// 동일한 충전량을 보유 혈액에는 음수, 탄창에는 양수로 전달
	Spec.Data->SetSetByCallerMagnitude(TBGameplayTags::Data_Reload_BloodCost, -ReloadAmount);
	Spec.Data->SetSetByCallerMagnitude(TBGameplayTags::Data_Reload_LoadedGain, ReloadAmount);

	// // 두 자원 변경을 하나의 GE로 적용
	// ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	
	// 재장전 적용: 체력을 소모하고 탄약을 충전
	const FActiveGameplayEffectHandle AppliedHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

	// 재장전 적용에 성공했다면 실제 장전량 저장
	if (AppliedHandle.WasSuccessfullyApplied())
	{
		ASC->SetNumericAttributeBase(UTBPlayerAttributeSet::GetAmmoAtLastReloadAttribute(),Attributes->GetLoadedBlood());
	}
}

void UTBGameplayAbility_BloodAction::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ATBPlayer* Player = ActorInfo ? Cast<ATBPlayer>(ActorInfo->AvatarActor.Get()) : nullptr;

	// 필수 대상 및 에셋 확인
	if (!IsValid(Player) || !ThirdPersonMontage || !GetCostGameplayEffect())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 공격은 시작 시 비용 적용
	// 재장전은 완료 콜백까지 실제 비용 적용 보류
	if (bCommitOnActivate && !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 소유 플레이어 화면의 팔 몽타주 재생
	if (Player->IsLocallyControlled() && FirstPersonMontage)
	{
		if (USkeletalMeshComponent* FirstPersonMesh = Player->GetFirstPersonMesh())
		{
			if (UAnimInstance* AnimInstance = FirstPersonMesh->GetAnimInstance())
			{
				AnimInstance->Montage_Play(FirstPersonMontage);
			}
		}
	}

	// ASC를 통한 전신 몽타주 재생 및 네트워크 동기화
	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, ThirdPersonMontage, 1.0f, NAME_None, true);

	if (!MontageTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnMontageInterrupted);

	MontageTask->ReadyForActivation();

	// 추후 실제 공격 판정 등 서버 게임 로직 연결
}

void UTBGameplayAbility_BloodAction::OnMontageCompleted()
{
	if (!IsActive())
	{
		return;
	}

	if (!bCommitOnActivate)
	{
		// 재장전 자원 전환은 서버에서만 확정
		if (!CurrentActorInfo || !CurrentActorInfo->IsNetAuthority())
		{
			return;
		}

		// 완료 시 비용 및 충전 상태 재검사 후 GameplayEffect 적용
		if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
			return;
		}
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UTBGameplayAbility_BloodAction::OnMontageInterrupted()
{
	if (IsActive())
	{
		// 재장전은 Commit 이전 중단 시 자원 변경 없음
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void UTBGameplayAbility_BloodAction::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (!IsActive())
	{
		return;
	}

	ATBPlayer* Player = ActorInfo ? Cast<ATBPlayer>(ActorInfo->AvatarActor.Get()) : nullptr;

	// 정상 종료·취소 시 남아 있는 팔 몽타주 정리
	if (IsValid(Player) && Player->IsLocallyControlled() && FirstPersonMontage)
	{
		if (USkeletalMeshComponent* FirstPersonMesh = Player->GetFirstPersonMesh())
		{
			if (UAnimInstance* AnimInstance = FirstPersonMesh->GetAnimInstance())
			{
				if (AnimInstance->Montage_IsPlaying(FirstPersonMontage))
				{
					AnimInstance->Montage_Stop(0.1f, FirstPersonMontage);
				}
			}
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}