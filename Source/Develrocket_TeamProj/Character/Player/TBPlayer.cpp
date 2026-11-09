#include "TBPlayer.h"

#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "TBGameplayTags.h"
#include "AbilitySystem/TBAbilitySystemComponent.h"
#include "Math/RotationMatrix.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Framework/TBPlayerState.h"
#include "UI/TBPlayerHUD.h"

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

bool ATBPlayer::ApplyDamage(float Damage, AActor* DamageSource)
{
	return true;
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
	if (IsValid(CachedASC.Get()) && CachedASC->GetAvatarActor() == this)
	{
		// Controller 복제 이후 ASC에 저장된 관련 참조 갱신
		CachedASC->RefreshAbilityActorInfo();
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

	// OwnerActor: PlayerState / AvatarActor: 현재 Player
	InitializeAbilitySystem(TBPlayerState->GetAbilitySystemComponent(), TBPlayerState);

	// 서버에서 최초 한 번 자원 및 Ability 설정
	TBPlayerState->InitializeGameplayData();
	
	// 
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
	}
}

bool ATBPlayer::ReceiveAttackHit_Implementation(const FGameplayTag& DamageType, float Damage, AActor* DamageSource,
	const FHitResult& HitResult)
{
	return ApplyDamage(Damage, DamageSource);
}
