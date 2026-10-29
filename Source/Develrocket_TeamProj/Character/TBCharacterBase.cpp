#include "TBCharacterBase.h"

#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"

#include "Components/TBCharacterMovementComponent.h"
#include "TBGameplayTags.h"

// 기본 이동 컴포넌트를 프로젝트의 커스텀 클래스로 교체
ATBCharacterBase::ATBCharacterBase(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UTBCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
    PrimaryActorTick.bCanEverTick = false;

    // 네트워크에서 캐릭터와 이동 상태 복제
    bReplicates = true;
    SetReplicateMovement(true);

    GetCapsuleComponent()->InitCapsuleSize(34.0f, 88.0f);

    UCharacterMovementComponent* CharacterMovement = GetCharacterMovement();

    // 걷기·앉기 속도와 점프 중 이동 제어 설정
    CharacterMovement->MaxWalkSpeed = 450.0f;
    CharacterMovement->MaxWalkSpeedCrouched = 200.0f;

    CharacterMovement->JumpZVelocity = 420.0f;
    CharacterMovement->AirControl = 0.25f;

    // 앉기 허용과 자세별 캡슐·눈높이 설정
    CharacterMovement->GetNavAgentPropertiesRef().bCanCrouch = true;
    CharacterMovement->SetCrouchedHalfHeight(44.0f);

    BaseEyeHeight = 64.0f;
    CrouchedEyeHeight = 32.0f;
}

UAbilitySystemComponent* ATBCharacterBase::GetAbilitySystemComponent() const
{
    return CachedASC.Get();
}

void ATBCharacterBase::InitializeAbilitySystem(UAbilitySystemComponent* InASC, AActor* InOwnerActor)
{
    if (!IsValid(InASC) || !IsValid(InOwnerActor))
    {
        return;
    }

    // 다른 ASC로 교체하기 전에 기존 캐릭터 연결 정리
    if (CachedASC && CachedASC.Get() != InASC)
    {
        UninitializeAbilitySystem();
    }

    CachedASC = InASC;

    // ASC의 Owner와 실제 Ability 수행 대상인 현재 캐릭터를 ActorInfo에 등록
    if (CachedASC->GetOwnerActor() != InOwnerActor || CachedASC->GetAvatarActor() != this)
    {
        CachedASC->InitAbilityActorInfo(InOwnerActor, this);
    }
    else
    {
        // 컨트롤러 등 변경된 ActorInfo 정보 갱신
        CachedASC->RefreshAbilityActorInfo();
    }
}

void ATBCharacterBase::UninitializeAbilitySystem()
{
    // 이미 다른 Avatar로 전환된 ASC의 연결은 유지
    if (IsValid(CachedASC.Get()) && CachedASC->GetAvatarActor() == this)
    {
        // ASC의 Owner는 유지하고 현재 Pawn 연결만 해제
        CachedASC->SetAvatarActor(nullptr);
    }

    CachedASC = nullptr;
}

void ATBCharacterBase::UnPossessed()
{
    Super::UnPossessed();

    UninitializeAbilitySystem();
}

void ATBCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UninitializeAbilitySystem();

    Super::EndPlay(EndPlayReason);
}

bool ATBCharacterBase::IsMovementBlocked() const
{
    if (!IsValid(CachedASC.Get()) || CachedASC->GetAvatarActor() != this)
    {
        // ASC가 유효하지 않거나 현재 캐릭터가 Avatar로 연결되지 않은 동안 이동 차단 판정
        return true;
    }

    return CachedASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead)
        || CachedASC->HasMatchingGameplayTag(TBGameplayTags::State_Stunned)
        || CachedASC->HasMatchingGameplayTag(TBGameplayTags::State_MovementBlocked);
}

void ATBCharacterBase::StartPrimaryAttack()
{
    // 기본 공격 버튼을 눌렀을 때 호출
    // 추후 공격 Ability에 입력 시작 전달 또는 활성화 요청
    // 예: 권총 단발 발사, 자동소총 연사 시작, 차지 공격 충전 시작
}

void ATBCharacterBase::StopPrimaryAttack()
{
    // 기본 공격 버튼을 놓거나 입력이 취소됐을 때 호출
    // 추후 공격 Ability에 입력 해제 전달
    // 예: 자동소총 연사 중단, 차지 공격 충전 종료
    // 입력 해제 이후의 발동 또는 취소는 Ability에서 판단
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
