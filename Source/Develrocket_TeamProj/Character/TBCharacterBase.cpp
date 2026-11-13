#include "TBCharacterBase.h"

#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/TBBloodBarComponent.h"

#include "Components/TBCharacterMovementComponent.h"
#include "TBGameplayTags.h"
#include "AbilitySystem/TBAbilitySystemComponent.h"

// 기본 이동 컴포넌트를 프로젝트의 커스텀 클래스로 교체
ATBCharacterBase::ATBCharacterBase(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UTBCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
    PrimaryActorTick.bCanEverTick = false;

    // 네트워크에서 캐릭터와 이동 상태 복제
    bReplicates = true;
    SetReplicateMovement(true);

    GetCapsuleComponent()->InitCapsuleSize(34.0f, 88.0f);

    BloodBarComponent = CreateDefaultSubobject<UTBBloodBarComponent>(TEXT("BloodBarComponent"));
    BloodBarComponent->SetupAttachment(GetRootComponent());

    UCharacterMovementComponent* CharacterMovementComp = GetCharacterMovement();

    // 걷기·앉기 속도와 점프 중 이동 제어 설정
    CharacterMovementComp->MaxWalkSpeed = 750.0f;
    CharacterMovementComp->MaxWalkSpeedCrouched = 200.0f;

    CharacterMovementComp->JumpZVelocity = 420.0f;
    CharacterMovementComp->AirControl = 0.25f;

    // 앉기 허용과 자세별 캡슐·눈높이 설정
    CharacterMovementComp->GetNavAgentPropertiesRef().bCanCrouch = true;
    CharacterMovementComp->SetCrouchedHalfHeight(44.0f);

    // 앉기·일어서기 높이 설정
    BaseEyeHeight = 80.0f;
    CrouchedEyeHeight = 48.0f;
}

UAbilitySystemComponent* ATBCharacterBase::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent.Get();
}

UTBAbilitySystemComponent* ATBCharacterBase::GetTBAbilitySystemComponent() const
{
    return AbilitySystemComponent.Get();
}

void ATBCharacterBase::InitializeAbilitySystem(UAbilitySystemComponent* InASC, AActor* InOwnerActor)
{
    UTBAbilitySystemComponent* TBASC = Cast<UTBAbilitySystemComponent>(InASC);

    if (!IsValid(TBASC) || !IsValid(InOwnerActor))
    {
        return;
    }

    // 다른 ASC로 교체하기 전에 기존 캐릭터 연결 정리
    if (AbilitySystemComponent && AbilitySystemComponent.Get() != InASC)
    {
        UninitializeAbilitySystem();
    }

    AbilitySystemComponent = TBASC;

    // ASC의 소유 액터와 현재 캐릭터 연결
    if (AbilitySystemComponent->GetOwnerActor() != InOwnerActor || AbilitySystemComponent->GetAvatarActor() != this)
    {
        AbilitySystemComponent->InitAbilityActorInfo(InOwnerActor, this);
    }
    else
    {
        AbilitySystemComponent->RefreshAbilityActorInfo();
    }

    if (BloodBarComponent) BloodBarComponent->BindAbilitySystem(AbilitySystemComponent.Get());

    // 서버에서 고정 타입 태그 적용 및 클라이언트 복제
    if (HasAuthority() && CharacterTypeTag.IsValid())
    {
        AbilitySystemComponent->SetLooseGameplayTagCount(CharacterTypeTag, 1, EGameplayTagReplicationState::TagOnly);
    }
}

void ATBCharacterBase::UninitializeAbilitySystem()
{
    if (BloodBarComponent) BloodBarComponent->UnbindAbilitySystem();
    if (IsValid(AbilitySystemComponent.Get()) && AbilitySystemComponent->GetAvatarActor() == this)
    {
        // 이전 Pawn에서 실행 중인 전투 Ability 취소
        const FGameplayTagContainer CombatTags(TBGameplayTags::Ability_Combat);
        AbilitySystemComponent->CancelAbilities(&CombatTags);

        // ASC를 유지하고 현재 캐릭터의 Avatar 연결만 해제
        AbilitySystemComponent->SetAvatarActor(nullptr);
    }

    // 직접 소유한 ASC는 유지하고 PlayerState 등 외부 소유 ASC의 참조만 해제
    if (!IsValid(AbilitySystemComponent.Get()) || AbilitySystemComponent->GetOwner() != this)
    {
        AbilitySystemComponent = nullptr;
    }
}

void ATBCharacterBase::UnPossessed()
{
    Super::UnPossessed();

    // 직접 소유한 ASC는 빙의가 해제되어도 캐릭터와 함께 유지
    if (!IsValid(AbilitySystemComponent.Get()) || AbilitySystemComponent->GetOwner() != this)
    {
        UninitializeAbilitySystem();
    }
}

void ATBCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UninitializeAbilitySystem();

    Super::EndPlay(EndPlayReason);
}

bool ATBCharacterBase::IsMovementBlocked() const
{
    if (!IsValid(AbilitySystemComponent.Get()) || AbilitySystemComponent->GetAvatarActor() != this)
    {
        // ASC가 유효하지 않거나 현재 캐릭터가 Avatar로 연결되지 않은 동안 이동 차단 판정
        return true;
    }

    return AbilitySystemComponent->HasMatchingGameplayTag(TBGameplayTags::State_Dead)
        || AbilitySystemComponent->HasMatchingGameplayTag(TBGameplayTags::State_Stunned)
        || AbilitySystemComponent->HasMatchingGameplayTag(TBGameplayTags::State_MovementBlocked);
}

void ATBCharacterBase::StartPrimaryAttack()
{
    if (!IsLocallyControlled())
    {
        return;
    }

    if (UTBAbilitySystemComponent* ASC = GetTBAbilitySystemComponent())
    {
        ASC->TryActivateAbilityByTag(TBGameplayTags::Ability_Combat_PrimaryAttack);
    }
}

void ATBCharacterBase::StopPrimaryAttack()
{
    // 단발 공격은 입력 해제에 따른 추가 처리 없음
    // 연사·차지 공격 구현 시 입력 해제 전달 추가
}

void ATBCharacterBase::StartSkill()
{
    // 스킬 버튼을 눌렀을 때 호출
    // 추후 스킬 Ability에 입력 시작 전달 또는 활성화 요청
    // 예: 대시 즉시 실행, 차지 스킬 충전 시작, 유지형 스킬 사용 시작
}

void ATBCharacterBase::StopSkill()
{
    // 스킬 버튼을 놓거나 입력이 취소됐을 때 호출
    // 추후 스킬 Ability에 입력 해제 전달
    // 예: 차지 스킬 충전 종료, 유지형 스킬 사용 중단
    // 입력 해제 이후의 발동 또는 취소는 Ability에서 판단
}
