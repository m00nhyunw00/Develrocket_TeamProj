#include "AbilitySystem/Abilities/TBGameplayAbility_BloodAction.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/TBBloodAttributeSet.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/Player/TBPlayer.h"
#include "Components/SkeletalMeshComponent.h"
#include "TBGameplayTags.h"

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

UTBGameplayAbility_Reload::UTBGameplayAbility_Reload()
{
	SetAssetTags(FGameplayTagContainer(TBGameplayTags::Ability_Combat_Reload));

	// 재장전 완료 시 서버에서 자원 전환
	bCommitOnActivate = false;
}

bool UTBGameplayAbility_Reload::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	// 비용 GameplayEffect의 보유 혈액 소모 가능 여부 검사
	if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags))
	{
		return false;
	}

	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

	if (!ASC)
	{
		return false;
	}

	const UTBBloodAttributeSet* Attributes = ASC->GetSet<UTBBloodAttributeSet>();

	if (!Attributes)
	{
		return false;
	}

	// 충전 혈액이 없는 경우에만 고정량 재장전 허용
	return Attributes->GetLoadedBlood() <= KINDA_SMALL_NUMBER;
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