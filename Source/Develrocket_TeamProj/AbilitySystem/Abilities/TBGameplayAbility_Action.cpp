#include "AbilitySystem/Abilities/TBGameplayAbility_Action.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
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

    UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

    // Player·Monster 공통 실행 대상 검증
    if (!ActorInfo
        || !ActorInfo->AvatarActor.IsValid()
        || !IsValid(ASC)
        || ASC->GetAvatarActor() != ActorInfo->AvatarActor.Get())
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

    const bool bWaitForMontageEnd = ShouldWaitForMontageEnd();
    const bool bWaitForFirstPersonMontage = !ThirdPersonMontage && FirstPersonMontage && bWaitForMontageEnd;

    // 1인칭 단독 액션은 서버에서도 계산 가능한 정방향 단발 재생 사용
    if (bWaitForFirstPersonMontage
        && (!FMath::IsFinite(FirstPersonMontage->RateScale) || FirstPersonMontage->RateScale <= 0.0f
            || !FMath::IsFinite(FirstPersonMontage->GetPlayLength()) || FirstPersonMontage->GetPlayLength() <= 0.0f))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 로컬 플레이어의 선택적 1인칭 몽타주 재생
    if (FirstPersonMontage)
    {
        if (UAnimInstance* AnimInstance = GetFirstPersonAnimInstance())
        {
            const float PlayedDuration = AnimInstance->Montage_Play(FirstPersonMontage);

            if (!IsActive())
            {
                return;
            }

            if (PlayedDuration > 0.0f)
            {
                if (FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(FirstPersonMontage))
                {
                    PlayingFirstPersonAnimInstance = AnimInstance;
                    FirstPersonMontageInstanceID = MontageInstance->GetInstanceID();

                    if (bWaitForFirstPersonMontage)
                    {
                        MontageInstance->OnMontageEnded.BindUObject(this, &ThisClass::OnFirstPersonMontageEnded, FirstPersonMontageInstanceID);
                        MontageInstance->OnMontageBlendingOutStarted.BindUObject(this, &ThisClass::OnFirstPersonMontageBlendingOut, FirstPersonMontageInstanceID);
                    }
                }
            }

            // 유일한 몽타주 재생 실패 시 액션 취소
            if (bWaitForFirstPersonMontage && FirstPersonMontageInstanceID == INDEX_NONE)
            {
                EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
                return;
            }
        }
    }

    if (!ThirdPersonMontage)
    {
        // 몽타주 없는 액션과 완료를 기다리지 않는 액션의 종료는 자식에서 처리
        if (!bWaitForFirstPersonMontage || FirstPersonMontageInstanceID != INDEX_NONE)
        {
            return;
        }

        // 로컬 실행인데 1인칭 AnimInstance가 없는 경우 설정 오류로 취소
        if (ActorInfo->IsLocallyControlled())
        {
            EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
            return;
        }

        // 원격 플레이어의 서버 실행은 1인칭 메시 없이 원본 재생 시간 대기
        // 루프·섹션 변경·재생 속도 변경이 필요한 액션은 3인칭 몽타주를 기준으로 사용
        const float Duration = FirstPersonMontage->GetPlayLength() / FirstPersonMontage->RateScale;
        FirstPersonWaitTask = UAbilityTask_WaitDelay::WaitDelay(this, Duration);

        if (!FirstPersonWaitTask)
        {
            EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
            return;
        }

        FirstPersonWaitTask->OnFinish.AddDynamic(this, &ThisClass::OnMontageCompleted);
        FirstPersonWaitTask->ReadyForActivation();
        return;
    }

    // 완료를 기다리지 않는 액션은 몽타주 재생 후 자식으로 복귀
    if (!bWaitForMontageEnd)
    {
        const float PlayedDuration = ASC->PlayMontage(this, ActivationInfo, ThirdPersonMontage, 1.0f, NAME_None, 0.0f);

        if (PlayedDuration <= 0.0f)
        {
            EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        }

        return;
    }

    // ASC를 통한 공통 전신 몽타주 재생
    MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, ThirdPersonMontage,
        1.0f, NAME_None, true, 1.0f, 0.0f, ShouldAllowMontageInterruptAfterBlendOut());

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
    const ATBPlayer* Player = CurrentActorInfo ? Cast<ATBPlayer>(CurrentActorInfo->AvatarActor.Get()) : nullptr;

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

void UTBGameplayAbility_Action::OnFirstPersonMontageEnded(UAnimMontage* Montage, bool bInterrupted, int32 MontageInstanceID)
{
    // 이전 실행에서 지연 전달된 콜백 제외
    if (!IsActive() || MontageInstanceID != FirstPersonMontageInstanceID)
    {
        return;
    }

    FirstPersonMontageInstanceID = INDEX_NONE;

    if (bInterrupted)
    {
        OnMontageInterrupted();
    }
    else
    {
        OnMontageCompleted();
    }
}

void UTBGameplayAbility_Action::OnFirstPersonMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted, int32 MontageInstanceID)
{
    if (bInterrupted)
    {
        OnFirstPersonMontageEnded(Montage, true, MontageInstanceID);
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

    // 완료 대기 액션 또는 취소된 액션의 1인칭 몽타주 정리
    const bool bShouldStopMontage = ShouldWaitForMontageEnd() || bWasCancelled;

    // 이번 액션에서 재생한 인스턴스만 정리하고 종료 중 콜백 재진입 방지
    if (UAnimInstance* AnimInstance = PlayingFirstPersonAnimInstance.Get())
    {
        if (FAnimMontageInstance* MontageInstance = AnimInstance->GetMontageInstanceForID(FirstPersonMontageInstanceID))
        {
            if (MontageInstance->OnMontageEnded.IsBoundToObject(this))
            {
                MontageInstance->OnMontageEnded.Unbind();
            }

            if (MontageInstance->OnMontageBlendingOutStarted.IsBoundToObject(this))
            {
                MontageInstance->OnMontageBlendingOutStarted.Unbind();
            }

            if (bShouldStopMontage && AnimInstance->GetActiveInstanceForMontage(MontageInstance->Montage) == MontageInstance)
            {
                AnimInstance->Montage_Stop(0.1f, MontageInstance->Montage);
            }
        }
    }

    PlayingFirstPersonAnimInstance.Reset();
    FirstPersonMontageInstanceID = INDEX_NONE;

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

    MontageTask = nullptr;
    FirstPersonWaitTask = nullptr;
}
