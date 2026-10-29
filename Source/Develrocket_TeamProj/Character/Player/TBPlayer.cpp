#include "Character/Player/TBPlayer.h"

#include "AbilitySystemComponent.h"
#include "EnhancedInputComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Microsoft/COMPointer.h"
#include "Player/TBPlayerState.h"

class ATBPlayerState;

ATBPlayer::ATBPlayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;
	
	GetCharacterMovement()->bOrientRotationToMovement = false;
	
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetRootComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, BaseEyeHeight));
	FirstPersonCamera->bUsePawnControlRotation = true;
	
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonMesh"));
	FirstPersonMesh->SetupAttachment(FirstPersonCamera);
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FirstPersonMesh->SetGenerateOverlapEvents(false);
	FirstPersonMesh->SetCastShadow(false);

	// 소유 플레이어에게 전신 메시 숨김
	GetMesh()->SetOwnerNoSee(true);
}

// BeginPlay보다 먼저 호출됨
void ATBPlayer::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	
	// 서버에서 PlayerState ASC 연결
	InitializeFromPlayerState();
}

void ATBPlayer::BeginPlay()
{
	Super::BeginPlay();
	
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
	
	if (IsValid(CachedASC.Get()) && CachedASC->GetAvatarActor() == this)
	{
		// Controller 복제 이후 ASC에 저장된 관련 참조 갱신
		CachedASC->RefreshAbilityActorInfo();
	}
}

void ATBPlayer::PawnClientRestart()
{
	Super::PawnClientRestart();
	
	InitializeFromPlayerState();
}

void ATBPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    UEnhancedInputComponent* EnhancedInput = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);

	if (MoveAction)
	{
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ATBPlayer::InputMove);
	}

	if (LookAction)
	{
		EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &ATBPlayer::InputLook);
	}

	if (JumpAction)
	{
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &ATBPlayer::InputJumpStarted);
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &ATBPlayer::InputJumpStopped);
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Canceled, this, &ATBPlayer::InputJumpStopped);
	}

	if (CrouchAction)
	{
		EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Started, this, &ATBPlayer::InputCrouchStarted);
		EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Completed, this, &ATBPlayer::InputCrouchStopped);
		EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Canceled, this, &ATBPlayer::InputCrouchStopped);
	}

	if (PrimaryAttackAction)
	{
		EnhancedInput->BindAction(PrimaryAttackAction, ETriggerEvent::Started, this, &ATBPlayer::InputPrimaryAttackStarted);
		EnhancedInput->BindAction(PrimaryAttackAction, ETriggerEvent::Completed, this, &ATBPlayer::InputPrimaryAttackStopped);
		EnhancedInput->BindAction(PrimaryAttackAction, ETriggerEvent::Canceled, this, &ATBPlayer::InputPrimaryAttackStopped);
	}

	if (SkillAction)
	{
		EnhancedInput->BindAction(SkillAction, ETriggerEvent::Started, this, &ATBPlayer::InputSkillStarted);
	}
}

void ATBPlayer::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
}

void ATBPlayer::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
}

void ATBPlayer::InitializeFromPlayerState()
{
	ATBPlayerState* PlayerState = GetPlayerState<ATBPlayerState>();

	if (!IsValid(PlayerState))
	{
		UninitializeAbilitySystem();
		return;
	}

	InitializeAbilitySystem(PlayerState->GetAbilitySystemComponent(), PlayerState);
}

void ATBPlayer::UpdateCameraHeight()
{
}

void ATBPlayer::InputMove(const FInputActionValue& Value)
{
	if (!Controller || IsMovementBlocked())
	{
		return;
	}

	const FVector2D Input = Value.Get<FVector2D>();

	const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);

	const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

	const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(Forward, Input.Y);
	AddMovementInput(Right, Input.X);
}

void ATBPlayer::InputLook(const FInputActionValue& Value)
{
}

void ATBPlayer::InputJumpStarted()
{
}

void ATBPlayer::InputJumpStopped()
{
}

void ATBPlayer::InputCrouchStarted()
{
}

void ATBPlayer::InputCrouchStopped()
{
}

void ATBPlayer::InputPrimaryAttackStarted()
{
}

void ATBPlayer::InputPrimaryAttackStopped()
{
}

void ATBPlayer::InputSkillStarted()
{
}
