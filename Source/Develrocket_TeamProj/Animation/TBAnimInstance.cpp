#include "TBAnimInstance.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

void UTBAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// 소유 캐릭터가 없는 프리뷰 상태에서도 이전 값이 남지 않도록 초기화
	GroundSpeed = 0.0f;
	MovementDirection = 0.0f;
	VerticalSpeed = 0.0f;
	AimPitch = 0.0f;
	bIsFalling = false;
	bIsCrouching = false;

	const ACharacter* OwnerCharacter = Cast<ACharacter>(TryGetPawnOwner());

	if (!IsValid(OwnerCharacter))
	{
		return;
	}
	
	// 캐릭터의 조준 방향과 몸체 방향 사이의 상대 회전 계산
	const FRotator RelativeAimRotation = (OwnerCharacter->GetBaseAimRotation() - OwnerCharacter->GetActorRotation()).GetNormalized();

	// 위쪽은 양수, 아래쪽은 음수인 Pitch를 조준 BS 범위로 제한
	AimPitch = FMath::Clamp(RelativeAimRotation.Pitch, -90.0f, 90.0f);

	const UCharacterMovementComponent* MovementComponent = OwnerCharacter->GetCharacterMovement();

	if (!IsValid(MovementComponent))
	{
		return;
	}

	const FVector Velocity = OwnerCharacter->GetVelocity();

	GroundSpeed = Velocity.Size2D();
	VerticalSpeed = Velocity.Z;

	bIsFalling = MovementComponent->IsFalling();
	bIsCrouching = OwnerCharacter->IsCrouched();

	// 정지 상태의 미세한 속도 오차로 이동 방향이 흔들리는 현상 방지
	if (GroundSpeed > KINDA_SMALL_NUMBER)
	{
		// 월드 속도를 캐릭터 기준으로 변환하여 전후좌우 이동 방향 계산
		const FVector LocalVelocity = OwnerCharacter->GetActorRotation().UnrotateVector(Velocity);

		// 전방 0도, 오른쪽 90도, 왼쪽 -90도를 기준으로 이동 각도 산출
		MovementDirection = FMath::RadiansToDegrees(FMath::Atan2(LocalVelocity.Y, LocalVelocity.X));
	}
}
