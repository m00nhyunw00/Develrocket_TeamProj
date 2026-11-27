#include "TBPlayer.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "TBGameplayTags.h"
#include "AbilitySystem/TBAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/TBAttributeSetBase.h"
#include "Character/Monster/TBMonster.h"
#include "Character/Human/TBCitizen.h"
#include "Math/RotationMatrix.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Framework/TBPlayerState.h"
#include "Framework/TBLevelManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "UI/TBPlayerHUD.h"
#include "Components/TBInteractionComponent.h"
#include "Interface/TBInteractInterface.h"

// 컴포넌트 및 회전 설정 ----------------------

ATBPlayer::ATBPlayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PulseMaterial(
		TEXT("/Game/_Game/Materials/Outline/M_PP_BloodPulse.M_PP_BloodPulse"));
	BloodPulseMaterial = PulseMaterial.Object;
	
	
	// Controller의 좌우 회전만 캐릭터 몸체에 반영
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	// 이동 방향과 관계없이 시선 방향으로 몸체 회전
	GetCharacterMovement()->bOrientRotationToMovement = false;
	
	// 시선 앞 상호작용 대상 탐지
	InteractionComponent = CreateDefaultSubobject<UTBInteractionComponent>(TEXT("InteractionComponent"));

	// 캡슐을 기준으로 1인칭 카메라 배치
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetRootComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, BaseEyeHeight));
	FirstPersonCamera->bUsePawnControlRotation = true;

	// 카메라에 1인칭 메시 부착
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonMesh"));
	FirstPersonMesh->SetupAttachment(FirstPersonCamera);
	
	// 1인칭 총: 소유 플레이어에게만 표시
	FirstPersonWeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FirstPersonWeaponMesh"));
	FirstPersonWeaponMesh->SetupAttachment(FirstPersonMesh);
	FirstPersonWeaponMesh->SetOnlyOwnerSee(true);
	FirstPersonWeaponMesh->SetOwnerNoSee(false);
	FirstPersonWeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FirstPersonWeaponMesh->SetGenerateOverlapEvents(false);
	FirstPersonWeaponMesh->SetCastShadow(false);

	// 3인칭 총: 소유 플레이어에게 숨기고 다른 플레이어에게 표시
	ThirdPersonWeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ThirdPersonWeaponMesh"));
	ThirdPersonWeaponMesh->SetupAttachment(GetMesh());
	ThirdPersonWeaponMesh->SetOnlyOwnerSee(false);
	ThirdPersonWeaponMesh->SetOwnerNoSee(true);
	ThirdPersonWeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ThirdPersonWeaponMesh->SetGenerateOverlapEvents(false);

	// 발바닥이 원점인 전신 메시를 위한 초기 배치
	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));

	ConfigurePlayerMeshes();
}

void ATBPlayer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!IsLocallyControlled() || !FirstPersonCamera)
	{
		return;
	}

	FVector CameraLocation = FirstPersonCamera->GetRelativeLocation();

	// 현재 자세의 눈높이를 목표로 카메라 높이 보간
	CameraLocation.Z = FMath::FInterpTo(CameraLocation.Z, BaseEyeHeight, DeltaSeconds, CameraHeightInterpSpeed);

	FirstPersonCamera->SetRelativeLocation(CameraLocation);
	UpdateBloodPulseVisual();
}

bool ATBPlayer::TryGetFirstPersonCameraLocation(FVector& OutLocation) const
{
	if (!IsValid(FirstPersonCamera))
	{
		return false;
	}

	OutLocation = FirstPersonCamera->GetComponentLocation();
	return true;
}

bool ATBPlayer::IsDead() const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();

	return bPawnDead || (HasAuthority() && IsValid(ASC) && ASC->GetAvatarActor() == this
		&& ASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead));
}

void ATBPlayer::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATBPlayer, bPawnDead);
}

void ATBPlayer::Die(ATBPlayerState* Killer)
{
	ATBPlayerState* PS = GetPlayerState<ATBPlayerState>();
	UAbilitySystemComponent* PlayerASC = GetAbilitySystemComponent();

	if (!HasAuthority() || !IsValid(PS)
		|| !PS->IsGameplayDataInitialized()
		|| !IsValid(PlayerASC)
		|| PlayerASC != PS->GetAbilitySystemComponent()
		|| PlayerASC->GetAvatarActor() != this
		|| IsDead())
	{
		return;
	}

	const TSubclassOf<UGameplayEffect> DeathEffectClass = PS->GetDeathEffectClass();

	if (!DeathEffectClass)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: DeathEffectClass is not set."), *GetName());
		return;
	}

	const FGameplayEffectSpecHandle Spec = PlayerASC->MakeOutgoingSpec(DeathEffectClass, 1.0f, PlayerASC->MakeEffectContext());

	if (!Spec.IsValid())
	{
		return;
	}

	const FActiveGameplayEffectHandle AppliedHandle = PlayerASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

	if (!AppliedHandle.WasSuccessfullyApplied() || !IsDead())
	{
		if (AppliedHandle.IsValid())
		{
			PlayerASC->RemoveActiveGameplayEffect(AppliedHandle);
		}

		UE_LOG(LogTemp, Error, TEXT("%s: Death GE failed to grant State.Dead."), *GetName());
		return;
	}

	PS->ForceNetUpdate();
	ForceNetUpdate();

	if (ATBLevelManager* LevelManager = Cast<ATBLevelManager>(
		UGameplayStatics::GetActorOfClass(this, ATBLevelManager::StaticClass())))
	{
		LevelManager->RecordPlayerDeath(*PS, Killer);
		LevelManager->QueuePlayerRespawn(*this, AppliedHandle);
	}
}

void ATBPlayer::HandleDeadTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (!HasAuthority() || NewCount <= 0 || bPawnDead)
	{
		return;
	}

	UTBAbilitySystemComponent* PlayerASC = GetTBAbilitySystemComponent();

	if (!IsValid(PlayerASC) || PlayerASC->GetAvatarActor() != this)
	{
		return;
	}

	// 취소 콜백 등에 의한 중복 처리 방지
	bPawnDead = true;
	PlayerASC->CancelAllAbilities();
	OnRep_PawnDead();

	// The death ability runs on the server and owns the death animation.
	if (!PlayerASC->TryActivateAbilityByTag(TBGameplayTags::Ability_Death))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: Death ability activation failed."), *GetName());
	}
}

void ATBPlayer::OnRep_PawnDead()
{
	SetCharacterOutlineSuppressed(bPawnDead);
	if (!bPawnDead)
	{
		return;
	}

	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	// 카메라 회전이 몸체에 반영되지 않도록 설정
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (IsValid(FirstPersonMesh.Get()))
	{
		FirstPersonMesh->SetHiddenInGame(true, true);
	}

	if (IsValid(FirstPersonWeaponMesh.Get()))
	{
		FirstPersonWeaponMesh->SetHiddenInGame(true, true);
	}
}

void ATBPlayer::StartCameraRecoil(
	float InPitch, float InRiseTime, float InReturnTime)
{
	if (!IsLocallyControlled() || IsDead() || !GetWorld())
	{
		return;
	}

	// 이번 발사의 조준 반동 배율
	const float AimScale = IsAiming() ? FMath::Clamp(AimRecoilMultiplier, 0.0f, 1.0f) : 1.0f;

	// 어빌리티 기본 반동에 캐릭터·조준 배율 적용
	const float ShotPitch = FMath::Max(0.0f, InPitch) * FMath::Max(0.0f, RecoilMultiplier) * AimScale;

	if (ShotPitch <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// 현재 반동 위치에서 다음 반동으로 연결
	RecoilStartPitch = GetCameraRecoilPitch();

	// 조준 중 최대 누적 반동 제한
	const float ShotMaxRecoilPitch = FMath::Max(0.0f, MaxRecoilPitch) * AimScale;

	RecoilPeakPitch = FMath::Clamp(RecoilStartPitch + ShotPitch, 0.0f, ShotMaxRecoilPitch);

	// 이번 발사의 반동 상승·복귀 시간
	ActiveRecoilRiseTime = FMath::Max(InRiseTime, 0.01f);
	ActiveRecoilReturnTime = FMath::Max(InReturnTime, 0.01f);

	RecoilStartTime = GetWorld()->GetTimeSeconds();
}

void ATBPlayer::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	Super::CalcCamera(DeltaTime, OutResult);

	if (!IsLocallyControlled())
	{
		return;
	}

	if (CurrentViewFOV <= 0.0f)
	{
		CurrentViewFOV = OutResult.FOV;
	}

	const float TargetFOV = IsAiming() ? AimFieldOfView : OutResult.FOV;

	CurrentViewFOV = FMath::FInterpTo(CurrentViewFOV, TargetFOV, DeltaTime, AimInterpSpeed);

	OutResult.FOV = CurrentViewFOV;

	if (IsDead())
	{
		return;
	}

	// 발사 시 확정한 반동을 최종 카메라 회전에 반영
	OutResult.Rotation.Pitch += GetCameraRecoilPitch();
}

bool ATBPlayer::ApplyDamage(float Damage, AActor* DamageSource)
{
    return ReceiveAttackHit_Implementation(Damage, DamageSource, FHitResult());
}

// [서버] 피해 GE 적용 성공 직후 호출됨 (ATBCharacterBase::ReceiveAttackHit_Implementation)
void ATBPlayer::OnAttackApplied(float Damage, AActor* DamageSource)
{
    Super::OnAttackApplied(Damage, DamageSource);

    if (IsValid(DamageSource) && DamageSource != this)
    {
        ClientNotifyDamageTaken(DamageSource->GetActorLocation(), DamageSource);
    }
}

// [맞은 클라이언트] HUD로 전달만 하고, 각도 계산은 위젯이 매 프레임 수행
void ATBPlayer::ClientNotifyDamageTaken_Implementation(FVector_NetQuantize Origin, AActor* DamageSource)
{
    APlayerController* PC = GetController<APlayerController>();
    if (ATBPlayerHUD* PlayerHUD = PC ? PC->GetHUD<ATBPlayerHUD>() : nullptr)
    {
        PlayerHUD->ShowDamageIndicator(Origin, DamageSource);
    }
}

void ATBPlayer::ResetInputForDraining()
{
	// 이전 점프·앉기 요청 해제
	StopJumping();
	bCrouchInputHeld = false;
	UnCrouch();

	// 누적 이동 입력과 현재 이동 속도 정리
	ConsumeMovementInputVector();

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
	}
}

// PlayerState의 ASC가 초기화되어 현재 캐릭터를 Avatar로 사용 중인지 확인한다.
bool ATBPlayer::CanReceiveAttack(float Damage) const
{
    const ATBPlayerState* PS = GetPlayerState<ATBPlayerState>();
    UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
    return IsValid(PS) && PS->IsGameplayDataInitialized() && IsValid(ASC)
        && ASC == PS->GetAbilitySystemComponent() && ASC->GetAvatarActor() == this && !IsDead();
}

// 플레이어의 피해 GE 설정은 기존처럼 PlayerState에서 가져온다.
TSubclassOf<UGameplayEffect> ATBPlayer::GetAttackDamageEffect() const
{
    const ATBPlayerState* PS = GetPlayerState<ATBPlayerState>();
    return IsValid(PS) ? PS->GetDamageEffectClass() : nullptr;
}

FHitResult ATBPlayer::TraceHitscanShot(
    const FGameplayTag& AttackVisualCueTag,
    const FGameplayTag& DefaultImpactCueTag,
    float TraceRange,
    bool bDrawTrace,
    bool bEnhanced)
{
    FHitResult HitResult;

    // 서버 판정 및 사망 상태 검증
    if (!HasAuthority() || IsDead())
    {
        return HitResult;
    }

    const ATBPlayerState* PS = GetPlayerState<ATBPlayerState>();
    UAbilitySystemComponent* PlayerASC = GetAbilitySystemComponent();
    UWorld* World = GetWorld();

    // PlayerState 초기화 및 공격에 필요한 객체 검증
    if (!IsValid(PS) || !PS->IsGameplayDataInitialized()
        || !IsValid(PlayerASC) || !World || !GetController())
    {
        return HitResult;
    }

    // 현재 캐릭터와 PlayerState의 ASC 연결 검증
    if (PlayerASC != PS->GetAbilitySystemComponent()
        || PlayerASC->GetAvatarActor() != this)
    {
        return HitResult;
    }

    // 호출부에서 전달한 사거리 검증
    if (!FMath::IsFinite(TraceRange) || TraceRange <= 0.0f)
    {
        return HitResult;
    }

    // 서버의 눈 위치와 조준 방향
    FVector TraceStart;
    FRotator TraceRotation;
    GetActorEyesViewPoint(TraceStart, TraceRotation);

    const FVector TraceEnd =
        TraceStart + TraceRotation.Vector() * TraceRange;

    // 프로젝트의 HitscanTrace 채널로 첫 차단 지점 검사
    constexpr ECollisionChannel HitscanTraceChannel = ECC_GameTraceChannel2;

    const bool bBlockingHit =
        LineTraceByChannel(HitResult, TraceStart, TraceEnd, HitscanTraceChannel);

    // 명중 시 충돌 지점, 빗나가면 최대 사거리 끝점
    const FVector TrailEnd =
        bBlockingHit ? FVector(HitResult.ImpactPoint) : TraceEnd;

    // 명중 여부와 관계없이 끝점 확정 후 공격 연출을 한 번 실행
    if (AttackVisualCueTag.IsValid())
    {
        FGameplayCueParameters AttackVisualParameters;
        AttackVisualParameters.Location = TrailEnd;
        AttackVisualParameters.Instigator = this;
        AttackVisualParameters.EffectCauser = this;
        // 강화 버프 종료 후에도 발사 시점의 연출 정보 유지
        AttackVisualParameters.NormalizedMagnitude = bEnhanced ? 1.0f : 0.0f;

        // 서버에서 실행한 공격 Cue의 발사자 전달을 위한 예측 키 분리
        FScopedPredictionWindow CuePredictionWindow(
            PlayerASC, FPredictionKey(), false);

        PlayerASC->ExecuteGameplayCue(AttackVisualCueTag, AttackVisualParameters);
    }

    // 서버 트레이스 결과 디버그 표시
    if (bDrawTrace)
    {
        const FColor TraceColor =
            bBlockingHit ? FColor::Red : FColor::Green;

        DrawDebugLine(
            World, TraceStart, TrailEnd,
            TraceColor, false, 2.0f, 0, 1.5f);

        if (bBlockingHit)
        {
            DrawDebugPoint(
                World, HitResult.ImpactPoint,
                12.0f, FColor::Yellow, false, 2.0f);
        }
    }

    // 미명중 시 탄착 연출 없이 결과 반환
    if (!bBlockingHit)
    {
        return HitResult;
    }

    // 비생명체에 사용할 기본 탄착 Cue
    FGameplayTag ImpactCueTag = DefaultImpactCueTag;

    // 캐릭터에 설정된 혈액 Cue 선택
    if (const ATBCharacterBase* HitCharacter =
        Cast<ATBCharacterBase>(HitResult.GetActor()))
    {
        ImpactCueTag = HitCharacter->GetBloodImpactCueTag();

        // 혈액 Cue 미설정 시 기본 붉은 혈액 연출
        if (!ImpactCueTag.IsValid())
        {
            ImpactCueTag = TBGameplayTags::GameplayCue_Impact_Blood_Flesh_Red;
        }
    }

    if (ImpactCueTag.IsValid())
    {
        // 충돌 컴포넌트와 본 정보를 포함한 피격 정보
        FGameplayEffectContextHandle ImpactContext =
            PlayerASC->MakeEffectContext();

        ImpactContext.AddHitResult(HitResult, true);

        // 피격 위치 및 표면 방향 전달
        FGameplayCueParameters ImpactParameters(ImpactContext);
        ImpactParameters.Location = HitResult.ImpactPoint;
        ImpactParameters.Normal = HitResult.ImpactNormal;
        ImpactParameters.Instigator = this;
        ImpactParameters.EffectCauser = this;
        ImpactParameters.NormalizedMagnitude = bEnhanced ? 1.0f : 0.0f;

        // 서버에서 실행한 탄착 Cue의 예측 키 분리
        FScopedPredictionWindow ImpactPredictionWindow(
            PlayerASC, FPredictionKey(), false);

        PlayerASC->ExecuteGameplayCue(ImpactCueTag, ImpactParameters);
    }

    return HitResult;
}

bool ATBPlayer::GetAttackDamage(const FGameplayTag& AttackId, float& OutDamage) const
{
	OutDamage = 0.0f;

	const ATBPlayerState* PS = GetPlayerState<ATBPlayerState>();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();

	if (!HasAuthority() || !IsValid(PS)
		|| !PS->IsGameplayDataInitialized() || !IsValid(ASC)
		|| ASC != PS->GetAbilitySystemComponent()
		|| ASC->GetAvatarActor() != this)
	{
		return false;
	}

	return PS->GetAttackDamage(AttackId, OutDamage);
}

bool ATBPlayer::ApplyHitscanDamage(const FHitResult& HitResult, float Damage)
{
	return !IsActionBlocked() && ApplyAttackDamage(HitResult, Damage, true);
}

bool ATBPlayer::ApplyMeleeDamage(const FHitResult& HitResult, float Damage)
{
	return !IsActionBlocked() && ApplyAttackDamage(HitResult, Damage);
}

bool ATBPlayer::ApplyProjectileDamage(const FHitResult& HitResult, float Damage)
{
	return ApplyAttackDamage(HitResult, Damage, true);
}

bool ATBPlayer::ApplyAttackDamage(const FHitResult& HitResult, float Damage, bool bRangedAttack)
{
	if (!HasAuthority() || IsDead() || !FMath::IsFinite(Damage) || Damage <= 0.0f)
	{
		return false;
	}

	const ATBPlayerState* PS = GetPlayerState<ATBPlayerState>();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();

	if (!IsValid(PS) || !PS->IsGameplayDataInitialized()
		|| !IsValid(ASC) || !GetController()
		|| ASC != PS->GetAbilitySystemComponent()
		|| ASC->GetAvatarActor() != this)
	{
		return false;
	}

	AActor* Target = HitResult.GetActor();

	if (!IsValid(Target) || Target == this || !Target->Implements<UTBHitInterface>())
	{
		return false;
	}

	// 명중 시 PS에서 다시 조회하지 않고 전달받은 피해량 사용
	return bRangedAttack ? DispatchRangedAttackHit(*Target, Damage, this, HitResult)
		: ITBHitInterface::Execute_ReceiveAttackHit(Target, Damage, this, HitResult);
}

// 빙의 및 네트워크 초기화 ----------------------

void ATBPlayer::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 서버에서 PlayerState ASC 연결
	InitializeFromPlayerState();
}

bool ATBPlayer::TryGetWeaponSocketLocation(FName SocketName, FVector& OutLocation) const
{
	// weapon_end 소켓을 보유한 캐릭터 메시 선택
	// 본인은 1인칭 메시, 다른 플레이어는 3인칭 메시 사용
	const USkeletalMeshComponent* SocketMesh =
		IsLocallyControlled() ? FirstPersonMesh.Get() : GetMesh();

	if (!IsValid(SocketMesh) || !SocketMesh->DoesSocketExist(SocketName))
	{
		return false;
	}

	// 애니메이션이 반영된 소켓의 월드 좌표 반환
	OutLocation = SocketMesh->GetSocketLocation(SocketName);
	return true;
}

void ATBPlayer::BeginPlay()
{
	Super::BeginPlay();
	SetCharacterOutlineColor(1.0f, 0.0f, 0.0f);

	// 기존 Blueprint에 저장된 값과 관계없이 실행 시 표시 규칙 적용
	ConfigurePlayerMeshes();

	InitializeFromPlayerState();
	UpdateCameraHeight();
	
	InitializeWeaponMeshes();
}

void ATBPlayer::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// PlayerState 복제 후 클라이언트에서 ASC 연결
	InitializeFromPlayerState();
}

void ATBPlayer::OnRep_Controller()
{
	Super::OnRep_Controller();

	// 현재 캐릭터에 연결된 ASC만 갱신하여 다른 Avatar의 정보 변경 방지
	if (IsValid(AbilitySystemComponent.Get()) && AbilitySystemComponent->GetAvatarActor() == this)
	{
		// Controller 복제 이후 ASC에 저장된 관련 참조 갱신
		AbilitySystemComponent->RefreshAbilityActorInfo();
	}
	
	TryBindLocalHUD();
}

void ATBPlayer::PawnClientRestart()
{
	Super::PawnClientRestart();

	// 소유 클라이언트의 Pawn 재시작 시 ASC 연결 재확인
	InitializeFromPlayerState();
}

// Enhanced Input 바인딩 ----------------------

void ATBPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    UEnhancedInputComponent* EnhancedInput = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);

	// 이동·시점 입력이 트리거되는 동안 입력값을 지속적으로 반영
	if (MoveAction)
	{
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ATBPlayer::InputMove);
	}

	if (LookAction)
	{
		EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &ATBPlayer::InputLook);
	}

	// 점프 입력 시작 및 종료 연결
	// Completed와 Canceled 모두 해제 처리하여 입력 유지 상태 정리
	if (JumpAction)
	{
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &ATBPlayer::InputJumpStarted);
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &ATBPlayer::InputJumpStopped);
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Canceled, this, &ATBPlayer::InputJumpStopped);
	}

	// 누르는 동안 앉기, 입력 종료 시 일어서기
	if (CrouchAction)
	{
		EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Started, this, &ATBPlayer::InputCrouchStarted);
		EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Completed, this, &ATBPlayer::InputCrouchStopped);
		EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Canceled, this, &ATBPlayer::InputCrouchStopped);
	}

	// 기본 공격의 입력 시작·해제를 공통 캐릭터 함수에 전달
	if (PrimaryAttackAction)
	{
		EnhancedInput->BindAction(PrimaryAttackAction, ETriggerEvent::Started, this, &ATBPlayer::InputPrimaryAttackStarted);
		EnhancedInput->BindAction(PrimaryAttackAction, ETriggerEvent::Completed, this, &ATBPlayer::InputPrimaryAttackStopped);
		EnhancedInput->BindAction(PrimaryAttackAction, ETriggerEvent::Canceled, this, &ATBPlayer::InputPrimaryAttackStopped);
	}
	
	if (MeleeAction)
	{
		EnhancedInput->BindAction(MeleeAction, ETriggerEvent::Started, this, &ATBPlayer::InputMeleeStarted);
	}

	// 스킬의 입력 시작·해제를 공통 캐릭터 함수에 전달
	if (Skill1Action)
	{
		EnhancedInput->BindAction(Skill1Action, ETriggerEvent::Started, this, &ATBPlayer::InputSkill1Started);
		EnhancedInput->BindAction(Skill1Action, ETriggerEvent::Completed, this, &ATBPlayer::InputSkill1Stopped);
		EnhancedInput->BindAction(Skill1Action, ETriggerEvent::Canceled, this, &ATBPlayer::InputSkill1Stopped);
	}
	
	if (Skill2Action)
	{
		EnhancedInput->BindAction(Skill2Action, ETriggerEvent::Started, this, &ATBPlayer::InputSkill2Started);
		EnhancedInput->BindAction(Skill2Action, ETriggerEvent::Completed, this, &ATBPlayer::InputSkill2Stopped);
		EnhancedInput->BindAction(Skill2Action, ETriggerEvent::Canceled, this, &ATBPlayer::InputSkill2Stopped);
	}
	
	// 스킬의 입력 시작을 공통 캐릭터 함수에 전달
	if (ReloadAction)
	{
		EnhancedInput->BindAction(ReloadAction, ETriggerEvent::Started, this, &ATBPlayer::InputReloadStarted);
	}
	
	// 상호작용 입력 시작을 전달
	if (InteractAction)
	{
		EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &ATBPlayer::InputInteractStarted);
	}
	
	if (PauseAction)
	{
		EnhancedInput->BindAction(PauseAction, ETriggerEvent::Started, this, &ATBPlayer::InputPauseStarted);
	}
	
	if (LeaderboardAction)
	{
		EnhancedInput->BindAction(LeaderboardAction, ETriggerEvent::Started, this, &ATBPlayer::InputLeaderboardStarted);
		EnhancedInput->BindAction(LeaderboardAction, ETriggerEvent::Completed, this, &ATBPlayer::InputLeaderboardCompleted);
		EnhancedInput->BindAction(LeaderboardAction, ETriggerEvent::Canceled, this, &ATBPlayer::InputLeaderboardCompleted);
	}
	else
	{
		PlayerInputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &ATBPlayer::InputLeaderboardStarted);
		PlayerInputComponent->BindKey(EKeys::Tab, IE_Released, this, &ATBPlayer::InputLeaderboardCompleted);
	}

	if (SecondaryAction)
	{
		EnhancedInput->BindAction(SecondaryAction, ETriggerEvent::Started, this, &ATBPlayer::StartSecondaryAction);

		EnhancedInput->BindAction(SecondaryAction, ETriggerEvent::Completed, this, &ATBPlayer::StopSecondaryAction);

		EnhancedInput->BindAction(SecondaryAction, ETriggerEvent::Canceled, this, &ATBPlayer::StopSecondaryAction);
	}

	if (MobilityAction)
	{
		EnhancedInput->BindAction(MobilityAction, ETriggerEvent::Started, this, &ATBPlayer::StartMobility);

		EnhancedInput->BindAction(MobilityAction, ETriggerEvent::Completed, this, &ATBPlayer::StopMobility);

		EnhancedInput->BindAction(MobilityAction, ETriggerEvent::Canceled, this, &ATBPlayer::StopMobility);
	}
}

// 점프·앉기 상태 전환 ----------------------

void ATBPlayer::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	const UCharacterMovementComponent* MovementComponent = GetCharacterMovement();

	if (!IsLocallyControlled() || !FirstPersonCamera || !MovementComponent)
	{
		return;
	}

	// 캡슐 바닥을 유지하며 중심이 내려가는 경우 카메라의 순간적인 하강 보정
	if (MovementComponent->bCrouchMaintainsBaseLocation)
	{
		FVector CameraLocation = FirstPersonCamera->GetRelativeLocation();
		CameraLocation.Z += HalfHeightAdjust;
		FirstPersonCamera->SetRelativeLocation(CameraLocation);
	}
}

void ATBPlayer::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	const UCharacterMovementComponent* MovementComponent = GetCharacterMovement();

	if (!IsLocallyControlled() || !FirstPersonCamera || !MovementComponent)
	{
		return;
	}

	// 캡슐 바닥을 유지하며 중심이 올라가는 경우 카메라의 순간적인 상승 보정
	if (MovementComponent->bCrouchMaintainsBaseLocation)
	{
		FVector CameraLocation = FirstPersonCamera->GetRelativeLocation();
		CameraLocation.Z -= HalfHeightAdjust;
		FirstPersonCamera->SetRelativeLocation(CameraLocation);
	}
}

void ATBPlayer::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	// 착지 직후에는 앉기 의도만 설정하고 실제 자세 변경은 이동 컴포넌트에서 처리
	if (IsLocallyControlled() && bCrouchInputHeld && !IsMovementBlocked())
	{
		Crouch();
	}
}

void ATBPlayer::CheckJumpInput(float DeltaTime)
{
	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();

	if (bPressedJump && !IsMovementBlocked() && IsValid(MovementComponent))
	{
		// 서버와 소유 클라이언트 모두 점프 검사 전에 앉기 의도 해제
		UnCrouch();

		if (IsCrouched())
		{
			// 충돌 검사를 포함한 실제 앉기 해제를 점프 검사 전에 수행
			MovementComponent->UnCrouch(false);
		}
	}

	// 앉기 해제 결과를 기준으로 기본 점프 조건 검사 및 실행
	Super::CheckJumpInput(DeltaTime);
}

// GAS 연결 ----------------------

void ATBPlayer::InitializeFromPlayerState()
{
	ATBPlayerState* TBPlayerState = GetPlayerState<ATBPlayerState>();

	if (!IsValid(TBPlayerState))
	{
		UninitializeAbilitySystem();
		return;
	}

	InitializeAbilitySystem(TBPlayerState->GetAbilitySystemComponent(), TBPlayerState);

	TBPlayerState->InitializeDeathEvents();
	TBPlayerState->InitializeGameplayData();

	// 이벤트 등록 전에 도착한 사망 상태 반영
	if (HasAuthority() && IsDead())
	{
		HandleDeadTagChanged(TBGameplayTags::State_Dead, 1);
	}
	else if (HasAuthority() && TBPlayerState->IsGameplayDataInitialized())
	{
		UAbilitySystemComponent* PlayerASC = GetAbilitySystemComponent();

		if (IsValid(PlayerASC) && PlayerASC->GetNumericAttribute(UTBAttributeSetBase::GetBloodAttribute()) <= 0.0f)
		{
			Die();
		}
	}

	TryBindLocalHUD();
}

// 메시 및 카메라 설정 ----------------------

void ATBPlayer::ConfigurePlayerMeshes()
{
	// 화면에 보이지 않는 서버 전신 메시도 애니메이션 갱신
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	
	// 전신 메시는 소유 플레이어에게 숨기고 다른 플레이어에게 표시
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->SetOnlyOwnerSee(false);
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);

	// 1인칭 메시는 소유 플레이어에게만 표시
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->SetOwnerNoSee(false);
	FirstPersonMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);

	// 시각 표현용 1인칭 메시의 충돌·오버랩·그림자 비활성화
	FirstPersonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FirstPersonMesh->SetGenerateOverlapEvents(false);
	FirstPersonMesh->SetCastShadow(false);
}

void ATBPlayer::UpdateCameraHeight()
{
	if (!FirstPersonCamera)
	{
		return;
	}

	FVector CameraLocation = FirstPersonCamera->GetRelativeLocation();

	// 자세별 눈높이를 반영하면서 카메라의 수평 오프셋 유지
	CameraLocation.Z = BaseEyeHeight;

	FirstPersonCamera->SetRelativeLocation(CameraLocation);
}

// 이동·시점 입력 처리 ----------------------

void ATBPlayer::InputMove(const FInputActionValue& Value)
{
	// Controller 부재 또는 이동 제한 상태에서 이동 입력 차단
	if (!Controller || IsMovementBlocked())
	{
		return;
	}

	const FVector2D Input = Value.Get<FVector2D>();

	// 시점의 상하 기울기를 제외한 좌우 회전으로 수평 이동 방향 계산
	const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);

	const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

	const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	// 입력 Y축을 전후 이동, X축을 좌우 이동에 적용
	AddMovementInput(Forward, Input.Y);
	AddMovementInput(Right, Input.X);
}

void ATBPlayer::InputLook(const FInputActionValue& Value)
{
	// 행동 제한 상태에서 시점 회전 입력 차단
	if (IsActionBlocked())
	{
		return;
	}

	const FVector2D Input = Value.Get<FVector2D>();

	// 조준 상태에 따른 시점 입력 배율
	const float SensitivityMultiplier = IsAiming() ? FMath::Clamp(AimLookSensitivityMultiplier, 0.01f, 1.0f) : 1.0f;

	// 좌우·상하 시점 회전에 조준 감도 적용
	AddControllerYawInput(Input.X * SensitivityMultiplier);
	AddControllerPitchInput(Input.Y * SensitivityMultiplier);
}

// 점프 입력 처리 ----------------------

void ATBPlayer::InputJumpStarted()
{
	if (IsMovementBlocked())
	{
		return;
	}

	// 점프를 우선하기 위해 앉기 요청 해제
	UnCrouch();

	// CharacterMovement의 다음 점프 검사에서 처리할 입력 기록
	Jump();
}

void ATBPlayer::InputJumpStopped()
{
	StopJumping();
}

// 앉기 입력 처리 ----------------------

void ATBPlayer::InputCrouchStarted()
{
	const UCharacterMovementComponent* MovementComponent = GetCharacterMovement();

	// 이동 제한 상태에서는 앉기 입력 유지 상태도 기록하지 않음
	if (IsMovementBlocked() || !IsValid(MovementComponent))
	{
		return;
	}

	// 공중에서도 입력을 기억하여 착지 시 앉기 복구
	bCrouchInputHeld = true;

	// 같은 프레임에 점프가 요청되었거나 공중에 있으면 착지까지 앉기 보류
	if (bPressedJump || !MovementComponent->IsMovingOnGround())
	{
		return;
	}

	Crouch();
}

void ATBPlayer::InputCrouchStopped()
{
	bCrouchInputHeld = false;

	UnCrouch();
}

// 플레이어 공통 어빌리티 입력 전달 ----------------------

void ATBPlayer::PressAbilityInput(const FGameplayTag& InputTag)
{
    if (!IsLocallyControlled() || IsActionBlocked())
    {
        return;
    }

    if (UTBAbilitySystemComponent* ASC = GetTBAbilitySystemComponent())
    {
        ASC->AbilityInputTagPressed(InputTag);
    }
}

void ATBPlayer::ReleaseAbilityInput(const FGameplayTag& InputTag)
{
    // 행동 제한 상태에서도 입력 해제는 전달
    if (!IsLocallyControlled())
    {
        return;
    }

    if (UTBAbilitySystemComponent* ASC = GetTBAbilitySystemComponent())
    {
        ASC->AbilityInputTagReleased(InputTag);
    }
}

// 기본 공격 입력 요청 ----------------------

void ATBPlayer::StartPrimaryAttack()
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

void ATBPlayer::StopPrimaryAttack()
{
    // 단발 공격은 입력 해제에 따른 추가 처리 없음
    // 연사·차지 공격 구현 시 입력 해제 전달 추가
}

// 보조 행동 및 이동기 입력 요청 ----------------------

void ATBPlayer::StartSecondaryAction()
{
    bSecondaryInputHeld = true;
    PressAbilityInput(TBGameplayTags::Ability_Combat_SecondaryAction);
}

void ATBPlayer::StopSecondaryAction()
{
    bSecondaryInputHeld = false;
    ReleaseAbilityInput(TBGameplayTags::Ability_Combat_SecondaryAction);
}

bool ATBPlayer::IsAiming() const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();

	return IsValid(ASC)
		&& !IsActionBlocked()
		&& ASC->HasMatchingGameplayTag(TBGameplayTags::State_Aiming)
		&& (!IsLocallyControlled() || IsSecondaryInputHeld());
}

void ATBPlayer::StartMobility()
{
    PressAbilityInput(TBGameplayTags::Ability_Combat_Mobility);
}

void ATBPlayer::StopMobility()
{
    ReleaseAbilityInput(TBGameplayTags::Ability_Combat_Mobility);
}

// 근접 공격 및 스킬 입력 요청 ----------------------

void ATBPlayer::StartMeleeAttack()
{
    if (!IsLocallyControlled())
    {
        return;
    }

    if (UTBAbilitySystemComponent* ASC = GetTBAbilitySystemComponent())
    {
        ASC->TryActivateAbilityByTag(TBGameplayTags::Ability_Combat_Melee);
    }
}

void ATBPlayer::StartSkill1()
{
    PressAbilityInput(TBGameplayTags::Ability_Combat_Skill1);
}

void ATBPlayer::StopSkill1()
{
    ReleaseAbilityInput(TBGameplayTags::Ability_Combat_Skill1);
}

void ATBPlayer::StartSkill2()
{
    PressAbilityInput(TBGameplayTags::Ability_Combat_Skill2);
}

void ATBPlayer::StopSkill2()
{
    ReleaseAbilityInput(TBGameplayTags::Ability_Combat_Skill2);
}

// 기본 공격 입력 전달 ----------------------

void ATBPlayer::InputPrimaryAttackStarted()
{
	// 행동 제한 상태에서 기본 공격 시작 차단
	if (IsActionBlocked())
	{
		return;
	}
	
	StartPrimaryAttack();
}

void ATBPlayer::InputPrimaryAttackStopped()
{
	StopPrimaryAttack();
}

// 근접 공격 입력 전달 ----------------------

void ATBPlayer::InputMeleeStarted()
{
	// 행동 제한 상태에서 기본 공격 시작 차단
	if (IsActionBlocked())
	{
		return;
	}
	
	StartMeleeAttack();
}

// 스킬 입력 전달 ----------------------

void ATBPlayer::InputSkill1Started()
{
	// 행동 제한 상태에서 스킬 시작 차단
	if (IsActionBlocked())
	{
		return;
	}
	
	StartSkill1();
}

void ATBPlayer::InputSkill1Stopped()
{
	StopSkill1();
}

void ATBPlayer::InputSkill2Started()
{
	// 행동 제한 상태에서 스킬 시작 차단
	if (IsActionBlocked())
	{
		return;
	}
	
	StartSkill2();
}

void ATBPlayer::InputSkill2Stopped()
{
	StopSkill2();
}

void ATBPlayer::InputReloadStarted()
{
	if (!IsLocallyControlled())
	{
		return;
	}
	
	// 소유 플레이어의 행동 가능 상태에서만 재장전 요청
	if (!IsLocallyControlled() || IsActionBlocked())
	{
		return;
	}

	if (UTBAbilitySystemComponent* ASC = GetTBAbilitySystemComponent())
	{
		ASC->TryActivateAbilityByTag(TBGameplayTags::Ability_Combat_Reload);
	}
}

void ATBPlayer::InputInteractStarted()
{
	if (!IsLocallyControlled() || IsActionBlocked())
	{
		return;
	}
	
	// 소유 플레이어의 행동 가능 상태에서만 상호작용 요청
	if (!IsLocallyControlled() || IsActionBlocked())
	{
		return;
	}
	
	UTBAbilitySystemComponent* ASC = GetTBAbilitySystemComponent();

	// 지금 바라보는 상호작용 대상 (GA가 클라이언트에서 대상을 얻는 방법과 동일)
	AActor* Target = InteractionComponent ? InteractionComponent->GetFocusedActor() : nullptr;

	if (!ASC || !IsValid(Target) || !Target->Implements<UTBInteractInterface>())
	{
		return;
	}

	// 대상에게 "어떤 절차로 상호작용할지" 질문
	const FGameplayTag AbilityTag = ITBInteractInterface::Execute_GetInteractionAbilityTag(Target, this);

	// 빈 태그 = 이 대상은 지금 상호작용하지 않음
	if (!AbilityTag.IsValid())
	{
		return;
	}

	// 대상이 알려 준 태그로 발동 (예측 실행은 지금과 동일)
	ASC->TryActivateAbilityByTag(AbilityTag);
}

void ATBPlayer::InitializeWeaponMeshes()
{
	if (!IsValid(WeaponMeshAsset.Get()) || WeaponSocketName.IsNone())
	{
		return;
	}

	// 1인칭 팔의 소켓에 총 부착
	if (FirstPersonMesh->DoesSocketExist(WeaponSocketName))
	{
		FirstPersonWeaponMesh->SetStaticMesh(WeaponMeshAsset.Get());
		FirstPersonWeaponMesh->AttachToComponent(FirstPersonMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, WeaponSocketName);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("FirstPersonMesh에 소켓이 없습니다: %s"), *WeaponSocketName.ToString());
	}

	// 3인칭 전신의 소켓에 총 부착
	if (GetMesh()->DoesSocketExist(WeaponSocketName))
	{
		ThirdPersonWeaponMesh->SetStaticMesh(WeaponMeshAsset.Get());
		ThirdPersonWeaponMesh->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, WeaponSocketName);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("전신 Mesh에 소켓이 없습니다: %s"), *WeaponSocketName.ToString());
	}
}

void ATBPlayer::TryBindLocalHUD()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	ATBPlayerState* PS = GetPlayerState<ATBPlayerState>();

	if (!PC || !PC->IsLocalController() || !PS)
	{
		return;
	}

	if (ATBPlayerHUD* HUD = Cast<ATBPlayerHUD>(PC->GetHUD()))
	{
		HUD->BindAbilitySystem(PS->GetAbilitySystemComponent());
		HUD->BindInteraction(InteractionComponent);
	}
}



float ATBPlayer::GetCameraRecoilPitch() const
{
	if (!GetWorld() || RecoilStartTime < 0.0)
	{
		return 0.0f;
	}

	const float Elapsed = static_cast<float>(
		GetWorld()->GetTimeSeconds() - RecoilStartTime);

	// 현재 위치에서 최고점까지 상승
	if (Elapsed < ActiveRecoilRiseTime)
	{
		const float Alpha = FMath::Clamp(
			Elapsed / ActiveRecoilRiseTime, 0.0f, 1.0f);

		const float SmoothAlpha = Alpha * Alpha * (3.0f - 2.0f * Alpha);

		return FMath::Lerp(RecoilStartPitch, RecoilPeakPitch, SmoothAlpha);
	}

	// 최고점에서 반동이 없는 상태로 복귀
	if (Elapsed < ActiveRecoilRiseTime + ActiveRecoilReturnTime)
	{
		const float Alpha =
			(Elapsed - ActiveRecoilRiseTime) / ActiveRecoilReturnTime;

		const float SmoothAlpha = Alpha * Alpha * (3.0f - 2.0f * Alpha);

		return FMath::Lerp(RecoilPeakPitch, 0.0f, SmoothAlpha);
	}

	return 0.0f;
}

void ATBPlayer::InputPauseStarted()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	if (ATBPlayerHUD* HUD = Cast<ATBPlayerHUD>(PC->GetHUD()))
	{
		HUD->TogglePauseMenu();
	}
}

void ATBPlayer::InputLeaderboardStarted()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}
	
	if (ATBPlayerHUD* HUD = Cast<ATBPlayerHUD>(PC->GetHUD()))
	{
		HUD->ShowLeaderboard();
	}
}

void ATBPlayer::InputLeaderboardCompleted()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}
	
	if (ATBPlayerHUD* HUD = Cast<ATBPlayerHUD>(PC->GetHUD()))
	{
		HUD->HideLeaderboard();
	}
}
