#include "AbilitySystem/Abilities/TBGameplayAbility_Melee.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/TBCharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TBMeleeTraceComponent.h"
#include "Interface/TBHitInterface.h"
#include "TBGameplayTags.h"

UTBGameplayAbility_Melee::UTBGameplayAbility_Melee()
{
    // 근접 공격 입력에서 찾을 어빌리티 태그
    SetAssetTags(FGameplayTagContainer(TBGameplayTags::Ability_Combat_Melee));

    // 플레이어의 애니메이션 예측 실행 지원
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    ActivationOwnedTags.AddTag(TBGameplayTags::State_Combat);

    ActivationBlockedTags.AddTag(TBGameplayTags::State_Combat);
    ActivationBlockedTags.AddTag(TBGameplayTags::State_Dead);
    ActivationBlockedTags.AddTag(TBGameplayTags::State_Stunned);
    ActivationBlockedTags.AddTag(TBGameplayTags::State_Draining);
}

void UTBGameplayAbility_Melee::ActivateAbility(
    FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    ATBCharacterBase* Character = ActorInfo
        ? Cast<ATBCharacterBase>(ActorInfo->AvatarActor.Get())
        : nullptr;

    if (!IsValid(Character) || !ThirdPersonMontage
        || AllowedTargetClasses.IsEmpty())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 실제 판정 준비와 피해 구독은 서버에서만 수행
    if (ActorInfo->IsNetAuthority())
    {
        UTBMeleeTraceComponent* Trace =
            Character->GetMeleeTraceComponent();

        USkeletalMeshComponent* Mesh = Character->GetMesh();

        if (!Trace || !IsValid(Mesh)
            || !Trace->PrepareAttack(Character->GetMeleeTraceMesh()))
        {
            EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
            return;
        }

        ActiveTrace = Trace;
        AttackMesh = Mesh;

        // 전용 서버에서도 공격 소켓 위치 갱신
        PreviousTickOption = Mesh->VisibilityBasedAnimTickOption;
        Mesh->VisibilityBasedAnimTickOption =
            EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

        Trace->AddTickPrerequisiteComponent(Mesh);

        HitDelegateHandle = Trace->OnHit.AddUObject(
            this, &ThisClass::HandleMeleeHit);
    }

    // 구독을 마친 뒤 Action에서 몽타주 재생
    Super::ActivateAbility(
        Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UTBGameplayAbility_Melee::HandleMeleeHit(const FHitResult& HitResult)
{
    ATBCharacterBase* Attacker = Cast<ATBCharacterBase>(GetAvatarActorFromActorInfo());
    AActor* Target = HitResult.GetActor();

    if (!IsActive() || !IsValid(Attacker)
        || !Attacker->HasAuthority() || Attacker->IsActionBlocked()
        || !IsValid(Target) || Target == Attacker
        || !Target->Implements<UTBHitInterface>())
    {
        return;
    }

    // 공격별로 허용한 대상 클래스 확인
    const bool bAllowedTarget = AllowedTargetClasses.ContainsByPredicate(
        [Target](const TSubclassOf<AActor>& TargetClass)
        {
            return TargetClass.Get() && Target->IsA(TargetClass.Get());
        });

    if (!bAllowedTarget)
    {
        return;
    }

    UAbilitySystemComponent* TargetASC =
        UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);

    // 대상 ASC 연결 및 피격 가능 상태 확인
    if (!IsValid(TargetASC) || TargetASC->GetAvatarActor() != Target
        || TargetASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead)
        || TargetASC->HasMatchingGameplayTag(TBGameplayTags::State_Invulnerable))
    {
        return;
    }

    // 공격 캐릭터에서 피해량 조회 및 대상 피격 요청
    Attacker->PerformMeleeAttackHit(HitResult, DamageTypeTag);
}

void UTBGameplayAbility_Melee::EndAbility(
    FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility,
    bool bWasCancelled)
{
    if (UTBMeleeTraceComponent* Trace = ActiveTrace.Get())
    {
        Trace->OnHit.Remove(HitDelegateHandle);
        Trace->FinishAttack();

        if (USkeletalMeshComponent* Mesh = AttackMesh.Get())
        {
            Trace->RemoveTickPrerequisiteComponent(Mesh);
        }
    }

    HitDelegateHandle.Reset();
    ActiveTrace.Reset();

    if (USkeletalMeshComponent* Mesh = AttackMesh.Get())
    {
        Mesh->VisibilityBasedAnimTickOption = PreviousTickOption;
    }

    AttackMesh.Reset();

    Super::EndAbility(
        Handle, ActorInfo, ActivationInfo,
        bReplicateEndAbility, bWasCancelled);
}