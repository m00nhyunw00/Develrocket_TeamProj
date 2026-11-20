#include "AbilitySystem/Abilities/TBGameplayAbility_Melee.h"

#include "Character/TBCharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TBMeleeTraceComponent.h"
#include "TBGameplayTags.h"

UTBGameplayAbility_Melee::UTBGameplayAbility_Melee()
{
    // 기존 F키 공격의 기본 태그 및 피해량 조회 식별자
    SetAssetTags(FGameplayTagContainer(TBGameplayTags::Ability_Combat_Melee));
    AttackId = TBGameplayTags::Ability_Combat_Melee;

    // 어빌리티 실행 중 전투 상태 부여
    ActivationOwnedTags.AddTag(TBGameplayTags::State_Combat);
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

    // 캐릭터·판정용 몽타주·피해 대상 설정 검증
    if (!IsValid(Character)
        || !ThirdPersonMontage
        || AllowedTargetClasses.IsEmpty())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 서버에서만 판정 준비 및 타격 결과 구독
    if (ActorInfo->IsNetAuthority())
    {
        UTBMeleeTraceComponent* Trace = Character->GetMeleeTraceComponent();
        USkeletalMeshComponent* Mesh = Character->GetMesh();

        if (!IsValid(Trace)
            || !IsValid(Mesh)
            || !Trace->PrepareAttack(Character->GetMeleeTraceMesh()))
        {
            EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
            return;
        }

        ActiveTrace = Trace;
        AttackMesh = Mesh;

        // 공격 종료 시 복구할 기존 애니메이션 갱신 설정
        PreviousTickOption = Mesh->VisibilityBasedAnimTickOption;

        // 화면에 보이지 않는 서버 메시도 포즈와 소켓 위치 갱신
        Mesh->VisibilityBasedAnimTickOption =
            EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

        // 캐릭터 메시 갱신 이후 트레이스 실행
        Trace->AddTickPrerequisiteComponent(Mesh);

        // 트레이스에서 검출한 타격 결과 구독
        HitDelegateHandle =
            Trace->OnHit.AddUObject(this, &ThisClass::HandleMeleeHit);
    }

    // 부모 Attack의 피해량 확정 및 Action의 몽타주 재생
    // 실제 판정 시작은 몽타주의 ANS에서 요청
    Super::ActivateAbility(
        Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UTBGameplayAbility_Melee::HandleMeleeHit(const FHitResult& HitResult)
{
    // 서버 권한·공격자 상태·허용 대상·피해량 검증
    if (!IsAllowedAttackHit(HitResult))
    {
        return;
    }

    ATBCharacterBase* Attacker =
        Cast<ATBCharacterBase>(GetAvatarActorFromActorInfo());

    // 이번 어빌리티 실행에서 확정한 피해량 전달
    Attacker->ApplyMeleeDamage(HitResult, CurrentAttackDamage);
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
        // 타격 결과 구독 해제
        Trace->OnHit.Remove(HitDelegateHandle);

        // 판정 중단 및 공격 준비 상태 정리
        Trace->FinishAttack();

        // 캐릭터 메시와의 Tick 선행 관계 해제
        if (USkeletalMeshComponent* Mesh = AttackMesh.Get())
        {
            Trace->RemoveTickPrerequisiteComponent(Mesh);
        }
    }

    HitDelegateHandle.Reset();
    ActiveTrace.Reset();

    // 공격 전 애니메이션 갱신 설정 복구
    if (USkeletalMeshComponent* Mesh = AttackMesh.Get())
    {
        Mesh->VisibilityBasedAnimTickOption = PreviousTickOption;
    }

    AttackMesh.Reset();

    // 부모 어빌리티의 종료 처리
    Super::EndAbility(
        Handle, ActorInfo, ActivationInfo,
        bReplicateEndAbility, bWasCancelled);
}