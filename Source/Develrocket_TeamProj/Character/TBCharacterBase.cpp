#include "TBCharacterBase.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"
#include "Components/CapsuleComponent.h"
#include "Components/TBBloodBarComponent.h"
#include "GameFramework/PlayerController.h"

#include "Components/TBCharacterMovementComponent.h"
#include "TBGameplayTags.h"
#include "AbilitySystem/TBAbilitySystemComponent.h"
#include "Components/DecalComponent.h"
#include "Components/TBMeleeTraceComponent.h"
#include "Projectiles/TBProjectile.h"

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
    
    MeleeTraceComponent = CreateDefaultSubobject<UTBMeleeTraceComponent>(TEXT("MeleeTraceComponent"));
}

UAbilitySystemComponent* ATBCharacterBase::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent.Get();
}

UTBAbilitySystemComponent* ATBCharacterBase::GetTBAbilitySystemComponent() const
{
    return AbilitySystemComponent.Get();
}

UTBMeleeTraceComponent* ATBCharacterBase::GetMeleeTraceComponent() const
{
    return MeleeTraceComponent.Get();
}

bool ATBCharacterBase::GetAttackDamage(const FGameplayTag& AttackId, float& OutDamage) const
{
    OutDamage = 0.0f;
    return false;
}

bool ATBCharacterBase::ApplyHitscanDamage(const FHitResult& HitResult, float Damage)
{
    return false;
}

bool ATBCharacterBase::ApplyMeleeDamage(const FHitResult& HitResult, float Damage)
{
    return false;
}

bool ATBCharacterBase::ApplyProjectileDamage(const FHitResult& HitResult, float Damage)
{
    return false;
}

ATBProjectile* ATBCharacterBase::SpawnAttackProjectile(TSubclassOf<ATBProjectile> ProjectileClass, const FTransform& SpawnTransform)
{
    UWorld* World = GetWorld();

    if (!HasAuthority() || IsActionBlocked() || !World || !ProjectileClass || !SpawnTransform.IsValid())
    {
        return nullptr;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = this;
    SpawnParameters.Instigator = this;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    return World->SpawnActor<ATBProjectile>(ProjectileClass, SpawnTransform, SpawnParameters);
}

UPrimitiveComponent* ATBCharacterBase::GetMeleeTraceMesh() const
{
    return GetMesh();
}

void ATBCharacterBase::BeginMeleeTrace(USkeletalMeshComponent* SourceMesh)
{
    // 서버의 3인칭 메시에서만 판정 시작
    if (HasAuthority() && SourceMesh == GetMesh() && !IsActionBlocked() && MeleeTraceComponent)
    {
        MeleeTraceComponent->StartTrace();
    }
}


void ATBCharacterBase::EndMeleeTrace(USkeletalMeshComponent* SourceMesh)
{
    // 사망·스턴 상태에서도 종료 요청 허용
    if (HasAuthority() && SourceMesh == GetMesh() && MeleeTraceComponent)
    {
        MeleeTraceComponent->StopTrace();
    }
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

bool ATBCharacterBase::ReceiveAttackHit_Implementation(float Damage,
    AActor* DamageSource, const FHitResult& HitResult)
{
    // 피격 대상의 ASC와 자식 클래스가 지정한 피해 GE를 가져온다.
    UAbilitySystemComponent* TargetASC = GetAbilitySystemComponent();
    const TSubclassOf<UGameplayEffect> EffectClass = GetAttackDamageEffect();
    if (!HasAuthority() || !IsValid(TargetASC) || !EffectClass || !FMath::IsFinite(Damage)
        || Damage <= 0.0f || !CanReceiveAttack(Damage)) return false;

    // 공격자 ASC가 있으면 공격자 컨텍스트를 사용한다. Citizen은 기존처럼 자신의 ASC를 사용한다.
    UAbilitySystemComponent* SourceASC = IsValid(DamageSource)
        ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(DamageSource) : nullptr;
    UAbilitySystemComponent* SpecASC = UseSourceASCForAttackEffect() && IsValid(SourceASC) ? SourceASC : TargetASC;
    FGameplayEffectContextHandle Context = SpecASC->MakeEffectContext();
    if (IsValid(DamageSource))
    {
        Context.AddSourceObject(DamageSource);
        if (UseSourceASCForAttackEffect() && !IsValid(SourceASC)) Context.AddInstigator(DamageSource, DamageSource);
    }
    // 기존 피해 GE의 부호 규칙에 맞춰 Data.Damage에 음수 피해량을 전달한다.
    const FGameplayEffectSpecHandle Spec = SpecASC->MakeOutgoingSpec(EffectClass, 1.0f, Context);
    if (!Spec.IsValid()) return false;
    Spec.Data->SetSetByCallerMagnitude(TBGameplayTags::Data_Damage, -Damage);
    if (!TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()).WasSuccessfullyApplied()) return false;

    // GE 적용에 성공하면 공격자의 화면에서 이 대상을 마지막 공격 대상으로 선택한다.
    // 적용 성공이 실제 Blood 감소를 항상 보장하는 것은 아니다.
    const APawn* AttackerPawn = Cast<APawn>(DamageSource);
    if (IsValid(AttackerPawn) && Cast<APlayerController>(AttackerPawn->GetController()))
    {
        if (UTBBloodBarComponent* AttackerBar = AttackerPawn->FindComponentByClass<UTBBloodBarComponent>())
            AttackerBar->RecordAttackedTarget(this);
    }
    // 공통 처리 이후 몬스터의 Stagger, Citizen의 출혈 등 개별 반응을 실행한다.
    OnAttackApplied(Damage, DamageSource);
    return true;
}

bool ATBCharacterBase::CanReceiveAttack(float Damage) const
{
    return true;
}

TSubclassOf<UGameplayEffect> ATBCharacterBase::GetAttackDamageEffect() const
{
    return nullptr;
}

void ATBCharacterBase::OnAttackApplied(float Damage, AActor* DamageSource)
{
}

void ATBCharacterBase::ClearHitImpactDecals_Implementation()
{
    // 피격 Cue에서 생성한 데칼 식별 태그
    static const FName HitImpactDecalTag(TEXT("TB.HitImpactDecal"));

    // 제거 중 컴포넌트 목록 변경에 대비한 사전 수집
    TInlineComponentArray<UDecalComponent*> Decals(this);

    for (UDecalComponent* Decal : Decals)
    {
        // 유효한 피격 데칼만 제거
        if (IsValid(Decal) && Decal->ComponentHasTag(HitImpactDecalTag))
        {
            Decal->DestroyComponent();
        }
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
    // ASC가 유효하지 않거나 현재 캐릭터가 Avatar로 연결되지 않은 동안 이동 차단 판정
    if (!IsValid(AbilitySystemComponent.Get()) || AbilitySystemComponent->GetAvatarActor() != this)
    {
        return true;
    }

    return AbilitySystemComponent->HasMatchingGameplayTag(TBGameplayTags::State_Dead)
        || AbilitySystemComponent->HasMatchingGameplayTag(TBGameplayTags::State_Stunned)
        || AbilitySystemComponent->HasMatchingGameplayTag(TBGameplayTags::State_MovementBlocked);
}

bool ATBCharacterBase::IsActionBlocked() const
{
    // ASC 미연결 또는 다른 Avatar 연결 상태에서 행동 차단
    if (!IsValid(AbilitySystemComponent.Get()) || AbilitySystemComponent->GetAvatarActor() != this)
    {
        return true;
    }

    return AbilitySystemComponent->HasMatchingGameplayTag(TBGameplayTags::State_Dead)
        || AbilitySystemComponent->HasMatchingGameplayTag(TBGameplayTags::State_Stunned)
        || AbilitySystemComponent->HasMatchingGameplayTag(TBGameplayTags::State_Draining);
}

bool ATBCharacterBase::LineTraceByChannel(FHitResult& OutHit, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel) const
{
    // 이전 호출의 충돌 정보 초기화
    OutHit = FHitResult();

    const UWorld* World = GetWorld();

    // 월드 및 시작·끝 좌표의 유효성 검증
    if (!World || Start.ContainsNaN() || End.ContainsNaN())
    {
        return false;
    }

    // 기존 공격과 동일한 단순 충돌 검사
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TBCharacterLineTrace), false);

    // 호출한 캐릭터와 해당 캐릭터 소유 컴포넌트 제외
    QueryParams.AddIgnoredActor(this);

    // 지정한 채널에서 처음 발견한 블로킹 충돌 반환
    return World->LineTraceSingleByChannel(OutHit, Start, End, TraceChannel, QueryParams);
}
