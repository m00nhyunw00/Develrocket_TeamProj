#include "AbilitySystem/Abilities/TBGameplayAbility_Action.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/Player/TBPlayer.h"
#include "Components/SkeletalMeshComponent.h"

UTBGameplayAbility_Action::UTBGameplayAbility_Action()
{
    // ASC마다 별도의 Ability 인스턴스 사용
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    bRetriggerInstancedAbility = false;

    // 공통 기본값: 서버 실행
    // 플레이어의 예측 실행 여부는 자식에서 지정
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnlyTermination;
}

void UTBGameplayAbility_Action::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    if (!IsActive())
    {
        return;
    }

    UAbilitySystemComponent* ASC =
        ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

    // Player·Monster 공통 실행 대상 검증
    if (!ActorInfo
        || !ActorInfo->AvatarActor.IsValid()
        || !IsValid(ASC)
        || ASC->GetAvatarActor() != ActorInfo->AvatarActor.Get()
        || !ThirdPersonMontage)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 시작 시 비용·쿨다운 처리
    // 비용 GE가 없는 액션도 GAS 기본 Commit 처리를 사용할 수 있음
    if (bCommitOnActivate && !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 로컬 플레이어의 1인칭 몽타주 재생
    if (FirstPersonMontage)
    {
        if (UAnimInstance* AnimInstance = GetFirstPersonAnimInstance())
        {
            AnimInstance->Montage_Play(FirstPersonMontage);
        }
    }

    // ASC를 통한 공통 전신 몽타주 재생
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
}

UAnimInstance* UTBGameplayAbility_Action::GetFirstPersonAnimInstance() const
{
    const ATBPlayer* Player =
        CurrentActorInfo ? Cast<ATBPlayer>(CurrentActorInfo->AvatarActor.Get()) : nullptr;

    // 몬스터 또는 다른 플레이어에게는 1인칭 연출 생략
    if (!IsValid(Player) || !Player->IsLocallyControlled())
    {
        return nullptr;
    }

    USkeletalMeshComponent* FirstPersonMesh = Player->GetFirstPersonMesh();

    return IsValid(FirstPersonMesh) ? FirstPersonMesh->GetAnimInstance() : nullptr;
}

void UTBGameplayAbility_Action::OnMontageCompleted()
{
    if (IsActive())
    {
        HandleActionMontageCompleted();
    }
}

void UTBGameplayAbility_Action::OnMontageInterrupted()
{
    if (IsActive())
    {
        HandleActionMontageInterrupted();
    }
}

void UTBGameplayAbility_Action::HandleActionMontageCompleted()
{
    // 기본 완료 처리
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UTBGameplayAbility_Action::HandleActionMontageInterrupted()
{
    // 기본 중단 처리
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UTBGameplayAbility_Action::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (!IsActive())
    {
        return;
    }

    // 남아 있는 1인칭 몽타주 정리
    if (FirstPersonMontage)
    {
        if (UAnimInstance* AnimInstance = GetFirstPersonAnimInstance())
        {
            if (AnimInstance->Montage_IsPlaying(FirstPersonMontage))
            {
                AnimInstance->Montage_Stop(0.1f, FirstPersonMontage);
            }
        }
    }

    // 활성 태스크 종료 및 GAS 상태 정리
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

    MontageTask = nullptr;
}