#include "AbilitySystem/Abilities/TBGameplayAbility_Melee.h"

#include "AbilitySystemComponent.h"
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
    
    if (SequenceAvatar.Get() != Character)
    {
        SequenceAvatar = Character;
        NextSectionIndex = 0;
    }

    const int32 PlayedIndex = NextSectionIndex;
    LocalPlayedIndex = PlayedIndex;

    if (!AttackSections.IsEmpty())
    {
        const FName Section = GetActionMontageStartSection();

        if (Section.IsNone()
            || ThirdPersonMontage->GetSectionIndex(Section) == INDEX_NONE
            || (FirstPersonMontage && FirstPersonMontage->GetSectionIndex(Section) == INDEX_NONE))
        {
            EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
            return;
        }
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
    Super::ActivateAbility(  Handle, ActorInfo, ActivationInfo, TriggerEventData);

    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();

    if (AttackSections.IsEmpty() || !ActorInfo->IsNetAuthority() || !IsActive()
        || !IsValid(ASC) || ASC->GetAnimatingAbility() != this
        || ASC->GetCurrentMontage() != ThirdPersonMontage)
    {
        return;
    }

    NextSectionIndex = (PlayedIndex + 1) % AttackSections.Num();

    if (!ActorInfo->IsLocallyControlled() && Character->GetNetConnection())
    {
        ClientConfirmAttackSection(Character, ActivationInfo.GetActivationPredictionKey(), PlayedIndex);
    }
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

FName UTBGameplayAbility_Melee::GetActionMontageStartSection() const
{
    return AttackSections.IsValidIndex(NextSectionIndex) ? AttackSections[NextSectionIndex] : NAME_None;
}

void UTBGameplayAbility_Melee::ClientConfirmAttackSection_Implementation(AActor* Avatar, FPredictionKey PredictionKey, int32 PlayedIndex)
{
    if (!CurrentActorInfo || !IsValid(Avatar)
        || CurrentActorInfo->AvatarActor.Get() != Avatar
        || !AttackSections.IsValidIndex(PlayedIndex))
    {
        return;
    }

    SequenceAvatar = Avatar;
    NextSectionIndex = (PlayedIndex + 1) % AttackSections.Num();

    // 늦게 도착한 확인은 다음 타수만 갱신한다.
    // 현재 실행의 예측이 달랐을 때만 재생 중인 섹션을 보정한다.
    if (!IsActive() || LocalPlayedIndex == PlayedIndex
        || !(GetCurrentActivationInfo().GetActivationPredictionKey() == PredictionKey))
    {
        return;
    }

    LocalPlayedIndex = PlayedIndex;
    const FName Section = AttackSections[PlayedIndex];

    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    UAnimInstance* BodyAnim = CurrentActorInfo->GetAnimInstance();
    if (IsValid(ASC) && ASC->GetAnimatingAbility() == this
        && BodyAnim && BodyAnim->Montage_IsPlaying(ThirdPersonMontage))
    {
        BodyAnim->Montage_JumpToSection(Section, ThirdPersonMontage);
    }

    UAnimInstance* ArmsAnim = GetFirstPersonAnimInstance();
    if (FirstPersonMontage && ArmsAnim && ArmsAnim->Montage_IsPlaying(FirstPersonMontage))
    {
        ArmsAnim->Montage_JumpToSection(Section, FirstPersonMontage);
    }
}

