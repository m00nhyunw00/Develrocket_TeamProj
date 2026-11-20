#include "AbilitySystem/Abilities/TBGameplayAbility_Melee.h"

#include "Character/TBCharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TBMeleeTraceComponent.h"
#include "TBGameplayTags.h"

UTBGameplayAbility_Melee::UTBGameplayAbility_Melee()
{
    // 근접 공격 입력에서 찾을 어빌리티 태그
    SetAssetTags(FGameplayTagContainer(TBGameplayTags::Ability_Combat_Melee));

    // 기존 F키 공격의 피해량 조회 기본값; 기능별 BP에서 변경 가능
    AttackId = TBGameplayTags::Ability_Combat_Melee;

    ActivationOwnedTags.AddTag(TBGameplayTags::State_Combat);
}

void UTBGameplayAbility_Melee::ActivateAbility(
    FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    ATBCharacterBase* Character = ActorInfo ? Cast<ATBCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;

    if (!IsValid(Character) || !ThirdPersonMontage || AllowedTargetClasses.IsEmpty())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 실제 판정 준비와 피해 구독은 서버에서만 수행
    if (ActorInfo->IsNetAuthority())
    {
        UTBMeleeTraceComponent* Trace =  Character->GetMeleeTraceComponent();
        USkeletalMeshComponent* Mesh = Character->GetMesh();

        if (!Trace || !IsValid(Mesh) || !Trace->PrepareAttack(Character->GetMeleeTraceMesh()))
        {
            EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
            return;
        }

        ActiveTrace = Trace;
        AttackMesh = Mesh;

        // 전용 서버에서도 공격 소켓 위치 갱신
        PreviousTickOption = Mesh->VisibilityBasedAnimTickOption;
        Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

        Trace->AddTickPrerequisiteComponent(Mesh);

        HitDelegateHandle = Trace->OnHit.AddUObject(this, &ThisClass::HandleMeleeHit);
    }

    // 구독을 마친 뒤 Attack에서 피해량 확정 및 Action 몽타주 재생
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UTBGameplayAbility_Melee::HandleMeleeHit(const FHitResult& HitResult)
{
    if (!IsAllowedAttackHit(HitResult))
    {
        return;
    }

    ATBCharacterBase* Attacker = Cast<ATBCharacterBase>(GetAvatarActorFromActorInfo());
    Attacker->ApplyMeleeDamage(HitResult, CurrentAttackData);
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
