#include "TBPlayer.h"

#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Math/RotationMatrix.h"

#include "Player/TBPlayerState.h"

class ATBPlayerState;

// 컴포넌트 및 회전 설정 ----------------------

ATBPlayer::ATBPlayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Controller의 좌우 회전만 캐릭터 몸체에 반영
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	// 이동 방향에 따른 자동 회전을 비활성화하여 시선 방향 유지
	GetCharacterMovement()->bOrientRotationToMovement = false;

	// 기본 눈높이에 카메라 배치 및 Controller의 시점 회전 반영
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetRootComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, BaseEyeHeight));
	FirstPersonCamera->bUsePawnControlRotation = true;

	// 카메라를 따라 움직이며 소유 플레이어에게만 보이는 1인칭 팔 메시 구성
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonMesh"));
	FirstPersonMesh->SetupAttachment(FirstPersonCamera);
	FirstPersonMesh->SetOnlyOwnerSee(true);

	// 시각 표현용 팔 메시의 충돌·오버랩·그림자 비활성화
	FirstPersonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FirstPersonMesh->SetGenerateOverlapEvents(false);
	FirstPersonMesh->SetCastShadow(false);

	// 소유 플레이어에게 전신 메시 숨김
	GetMesh()->SetOwnerNoSee(true);
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

	// 현재 PlayerState를 기준으로 ASC 연결 시도
	InitializeFromPlayerState();
	UpdateCameraHeight();
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
}

// 앉기 상태 전환 ----------------------

void ATBPlayer::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
}

void ATBPlayer::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
}

// GAS 연결 및 카메라 높이 ----------------------

void ATBPlayer::InitializeFromPlayerState()
{
	ATBPlayerState* TBPlayerState = GetPlayerState<ATBPlayerState>();

	// PlayerState가 유효하지 않으면 기존 ASC 연결 정리
	if (!IsValid(TBPlayerState))
	{
		UninitializeAbilitySystem();
		return;
	}

	// PlayerState를 OwnerActor, 현재 캐릭터를 AvatarActor로 ASC 연결
	InitializeAbilitySystem(TBPlayerState->GetAbilitySystemComponent(), TBPlayerState);
}

void ATBPlayer::UpdateCameraHeight()
{
	if (!FirstPersonCamera)
	{
		return;
	}

	FVector Location = FirstPersonCamera->GetRelativeLocation();

	Location.Z = BaseEyeHeight;

	FirstPersonCamera->SetRelativeLocation(Location);
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
	// 이동 가능한 상태에서만 점프 시작
	if (!IsMovementBlocked())
	{
		Jump();
	}
}

void ATBPlayer::InputJumpStopped()
{
	// 이동 제한 여부와 관계없이 점프 입력 해제
	StopJumping();
}

// 앉기 입력 처리 ----------------------

void ATBPlayer::InputCrouchStarted()
{
	// 이동 가능한 상태에서만 앉기 요청
	if (!IsMovementBlocked())
	{
		Crouch();
	}
}

void ATBPlayer::InputCrouchStopped()
{
	// 앉기 입력 해제에 따른 일어서기 요청
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