#include "AbilitySystem/Abilities/TBGameplayAbility_DashAttack.h"

#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToForce.h"
#include "Character/Player/TBAmanai.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "TBGameplayTags.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"

UTBGameplayAbility_DashAttack::UTBGameplayAbility_DashAttack()
{
    SetAssetTags(FGameplayTagContainer(TBGameplayTags::Ability_Combat_Mobility));
    AttackId = TBGameplayTags::Ability_Combat_Mobility;

    // 질풍참 실행 중 일반 이동·점프 입력 차단
    ActivationOwnedTags.AddTag(TBGameplayTags::State_MovementBlocked);
}

void UTBGameplayAbility_DashAttack::OnActionCommitted()
{
    Super::OnActionCommitted();

    const ATBAmanai* Character = Cast<ATBAmanai>(GetAvatarActorFromActorInfo());
    PendingDashDirection = Character
        ? Character->GetBaseAimRotation().Vector().GetSafeNormal() : FVector::ZeroVector;
}

void UTBGameplayAbility_DashAttack::StartDashFromMeleeTrace(ATBAmanai& Character)
{
    if (!IsActive() || GetAvatarActorFromActorInfo() != &Character || PendingDashDirection.IsNearlyZero())
    {
        return;
    }

    // 이번 발동에서 저장한 시선 방향을 한 번만 사용
    const FVector Direction = PendingDashDirection;
    PendingDashDirection = FVector::ZeroVector;

    UCharacterMovementComponent* Movement = Character.GetCharacterMovement();
    if (!Movement || Movement->MovementMode == MOVE_None
        || !FMath::IsFinite(DashDistance) || DashDistance <= 0.0f
        || !FMath::IsFinite(DashDuration) || DashDuration <= 0.0f)
    {
        return;
    }

    DashingCharacter = &Character;
    Character.SetDashPawnPassThrough(true);
    Movement->SetMovementMode(MOVE_Flying);

    // 충돌 여부와 관계없이 같은 시간 동안 입력 시선 방향의 속도를 적용
    const float DashSpeed = DashDistance / DashDuration;
    UAbilityTask_ApplyRootMotionConstantForce* Task =
        UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
            this, TEXT("AmanaiDash"), Direction, DashSpeed, DashDuration,
            false, nullptr, ERootMotionFinishVelocityMode::SetVelocity,
            FVector::ZeroVector, 0.0f, false);

    if (!Task)
    {
        FinishDashTravel();
        return;
    }

    Task->OnFinish.AddDynamic(this, &ThisClass::FinishDashTravel);
    Task->ReadyForActivation();
}

void UTBGameplayAbility_DashAttack::FinishDashTravel()
{
    ATBAmanai* Character = DashingCharacter.Get();
    DashingCharacter.Reset();

    if (!Character)
    {
        return;
    }

    if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
    {
        // 사망 등으로 이미 다른 이동 모드가 적용된 경우 해당 상태 보존
        if (Movement->MovementMode == MOVE_Flying)
        {
            Movement->SetMovementMode(MOVE_Falling);
        }
    }

    Character->SetDashPawnPassThrough(false);
}

void UTBGameplayAbility_DashAttack::EndAbility(FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    PendingDashDirection = FVector::ZeroVector;
    FinishDashTravel();

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}