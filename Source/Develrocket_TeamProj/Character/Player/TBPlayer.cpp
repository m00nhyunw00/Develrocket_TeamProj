#include "TBPlayer.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "TBGameplayTags.h"
#include "AbilitySystem/TBAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/TBAttributeSetBase.h"
#include "Character/Monster/TBMonster.h"
#include "Math/RotationMatrix.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Framework/TBPlayerState.h"
#include "UI/TBPlayerHUD.h"
#include "Components/TBInteractionComponent.h"

class ATBPlayerState;

// 컴포넌트 및 회전 설정 ----------------------

ATBPlayer::ATBPlayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	
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
}

bool ATBPlayer::IsDead() const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();

	return IsValid(ASC) && ASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead);
}

void ATBPlayer::Die()
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
}

void ATBPlayer::HandleDeadTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount <= 0 || bDeathHandled)
	{
		return;
	}

	UTBAbilitySystemComponent* PlayerASC = GetTBAbilitySystemComponent();

	if (!IsValid(PlayerASC) || PlayerASC->GetAvatarActor() != this)
	{
		return;
	}

	// 취소 콜백 등에 의한 중복 처리 방지
	bDeathHandled = true;

	if (HasAuthority())
	{
		PlayerASC->CancelAllAbilities();
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

	// 부여된 어빌리티 중 Ability.Death 태그로 검색 및 실행
	if (HasAuthority())
	{
		if (!PlayerASC->TryActivateAbilityByTag(TBGameplayTags::Ability_Death))
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: Death ability activation failed."), *GetName());
		}
	}
}

void ATBPlayer::StartCameraRecoil(float InPitch, float InRiseTime, float InReturnTime)
{
	if (!IsLocallyControlled() || IsDead() || !GetWorld())
	{
		return;
	}

	// 어빌리티 기본 반동에 캐릭터 배율 적용
	const float ShotPitch = FMath::Max(0.0f, InPitch) * FMath::Max(0.0f, RecoilMultiplier);

	if (ShotPitch <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// 이전 반동의 현재 위치를 먼저 계산하여 연속 발사 시 튐 방지
	RecoilStartPitch = GetCameraRecoilPitch();

	RecoilPeakPitch = FMath::Clamp(RecoilStartPitch + ShotPitch,0.0f,FMath::Max(0.0f, MaxRecoilPitch));

	// 이번 어빌리티의 반동 시간 저장
	ActiveRecoilRiseTime = FMath::Max(InRiseTime, 0.01f);
	ActiveRecoilReturnTime = FMath::Max(InReturnTime, 0.01f);

	RecoilStartTime = GetWorld()->GetTimeSeconds();
}

void ATBPlayer::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	Super::CalcCamera(DeltaTime, OutResult);

	if (!IsLocallyControlled() || IsDead())
	{
		return;
	}

	// 기존 카메라 시점에 반동 각도 추가
	OutResult.Rotation.Pitch += GetCameraRecoilPitch();
}

bool ATBPlayer::ApplyDamage(float Damage, AActor* DamageSource)
{
	const ATBPlayerState* PS = GetPlayerState<ATBPlayerState>();
	UAbilitySystemComponent* PlayerASC = GetAbilitySystemComponent();

	// PlayerState 초기화 및 현재 캐릭터의 ASC 연결 검증
	if (!HasAuthority() || !IsValid(PS) || !PS->IsGameplayDataInitialized()
		|| !IsValid(PlayerASC) || PlayerASC != PS->GetAbilitySystemComponent()
		|| PlayerASC->GetAvatarActor() != this)
	{
		return false;
	}

	const TSubclassOf<UGameplayEffect> DamageEffectClass = PS->GetDamageEffectClass();

	// 사망 상태 및 피해 값 검증
	if (IsDead() || !FMath::IsFinite(Damage) || Damage <= 0.0f || !DamageEffectClass)
	{
		return false;
	}

	UAbilitySystemComponent* SourceASC = IsValid(DamageSource) ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(DamageSource) : nullptr;
	UAbilitySystemComponent* SpecASC = IsValid(SourceASC) ? SourceASC : PlayerASC;

	FGameplayEffectContextHandle Context = SpecASC->MakeEffectContext();

	if (IsValid(DamageSource))
	{
		Context.AddSourceObject(DamageSource);

		if (!IsValid(SourceASC))
		{
			Context.AddInstigator(DamageSource, DamageSource);
		}
	}

	const FGameplayEffectSpecHandle Spec = SpecASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, Context);

	if (!Spec.IsValid())
	{
		return false;
	}

	Spec.Data->SetSetByCallerMagnitude(TBGameplayTags::Data_Damage, -Damage);
	const bool bApplied = PlayerASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()).WasSuccessfullyApplied();

	return bApplied;
}

bool ATBPlayer::PerformPrimaryAttackTrace()
{
	// 서버 판정 및 사망 상태 검증
	if (!HasAuthority() || IsDead())
	{
		return false;
	}

	const ATBPlayerState* PS = GetPlayerState<ATBPlayerState>();
	UAbilitySystemComponent* PlayerASC = GetAbilitySystemComponent();
	UWorld* World = GetWorld();

	// PlayerState 초기화 및 ASC 유효성 검증
	if (!IsValid(PS) || !PS->IsGameplayDataInitialized() || !IsValid(PlayerASC) || !World || !GetController())
	{
		return false;
	}

	// 현재 캐릭터의 ASC 연결 검증
	if (PlayerASC != PS->GetAbilitySystemComponent() || PlayerASC->GetAvatarActor() != this)
	{
		return false;
	}

	const float Damage = PS->GetPrimaryAttackDamage();
	const float TraceRange = PS->GetPrimaryAttackRange();

	// 공격 설정 검증
	if (!FMath::IsFinite(Damage) || Damage <= 0.0f || !FMath::IsFinite(TraceRange) || TraceRange <= 0.0f)
	{
		return false;
	}

	// 서버의 눈 위치 및 조준 방향
	FVector TraceStart;
	FRotator TraceRotation;
	GetActorEyesViewPoint(TraceStart, TraceRotation);

	const FVector TraceEnd = TraceStart + TraceRotation.Vector() * TraceRange;

	// 공격자 자신과 자신의 컴포넌트 제외
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TBPrimaryAttackTrace), false);
	QueryParams.AddIgnoredActor(this);

	// 프로젝트 설정의 WeaponTrace 채널
	constexpr ECollisionChannel WeaponTraceChannel = ECC_GameTraceChannel2;

	FHitResult HitResult;
	const bool bBlockingHit = World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, WeaponTraceChannel, QueryParams);

	// 서버 판정 결과 표시
	if (PS->ShouldDrawPrimaryAttackTrace())
	{
		const FVector DebugEnd = bBlockingHit ? HitResult.ImpactPoint : TraceEnd;

		DrawDebugLine(World, TraceStart, DebugEnd, bBlockingHit ? FColor::Red : FColor::Green, false, 2.0f, 0, 1.5f);

		if (bBlockingHit)
		{
			DrawDebugPoint(World, HitResult.ImpactPoint, 12.0f, FColor::Yellow, false, 2.0f);
		}
	}

	if (!bBlockingHit)
	{
		return false;
	}

	AActor* HitActor = HitResult.GetActor();

	if (!IsValid(HitActor))
	{
		return false;
	}

	// Player 또는 Monster만 피해 대상 허용
	const bool bDamageTarget = HitActor->IsA<ATBPlayer>() || HitActor->IsA<ATBMonster>();

	if (!bDamageTarget || !HitActor->Implements<UTBHitInterface>())
	{
		return false;
	}

	// 피격 대상의 인터페이스 구현 호출
	const bool bApplied = ITBHitInterface::Execute_ReceiveAttackHit(HitActor, TBGameplayTags::Damage_Type_Bullet, Damage, this, HitResult);

	return bApplied;
}

// 빙의 및 네트워크 초기화 ----------------------

void ATBPlayer::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 서버에서 PlayerState ASC 연결
	InitializeFromPlayerState();
}

void ATBPlayer::BeginPlay()
{
	Super::BeginPlay();

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

	// 스킬의 입력 시작·해제를 공통 캐릭터 함수에 전달
	if (SkillAction)
	{
		EnhancedInput->BindAction(SkillAction, ETriggerEvent::Started, this, &ATBPlayer::InputSkillStarted);
		EnhancedInput->BindAction(SkillAction, ETriggerEvent::Completed, this, &ATBPlayer::InputSkillStopped);
		EnhancedInput->BindAction(SkillAction, ETriggerEvent::Canceled, this, &ATBPlayer::InputSkillStopped);
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
	if (IsDead())
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
	const FVector2D Input = Value.Get<FVector2D>();

	// 입력 X축을 좌우 시점, Y축을 상하 시점 회전에 적용
	AddControllerYawInput(Input.X);
	AddControllerPitchInput(Input.Y);
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
	// 공중에서도 입력을 기억하여 착지 시 앉기 복구
	bCrouchInputHeld = true;

	const UCharacterMovementComponent* MovementComponent = GetCharacterMovement();

	if (IsMovementBlocked() || !IsValid(MovementComponent))
	{
		return;
	}

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

// 기본 공격 입력 전달 ----------------------

void ATBPlayer::InputPrimaryAttackStarted()
{
	StartPrimaryAttack();
}

void ATBPlayer::InputPrimaryAttackStopped()
{
	StopPrimaryAttack();
}

// 스킬 입력 전달 ----------------------

void ATBPlayer::InputSkillStarted()
{
	StartSkill();
}

void ATBPlayer::InputSkillStopped()
{
	StopSkill();
}

void ATBPlayer::InputReloadStarted()
{
	if (!IsLocallyControlled())
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
	if (!IsLocallyControlled())
	{
		return;
	}
	
	if (UTBAbilitySystemComponent* ASC = GetTBAbilitySystemComponent())
	{
		ASC->TryActivateAbilityByTag(TBGameplayTags::Ability_Interaction);
	}
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

bool ATBPlayer::ReceiveAttackHit_Implementation(const FGameplayTag& DamageType, float Damage, AActor* DamageSource,
	const FHitResult& HitResult)
{
	return ApplyDamage(Damage, DamageSource);
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
