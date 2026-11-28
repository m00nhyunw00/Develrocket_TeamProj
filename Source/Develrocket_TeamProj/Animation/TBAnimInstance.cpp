#include "TBAnimInstance.h"

#include "Animation/AnimInstanceProxy.h"
#include "Character/Monster/TBMonster.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TBHunterCombatComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
const FName RifleMuzzleSocket(TEXT("weapon_r_muzzle"));
const FName UpperSpineBone(TEXT("spine_04"));
constexpr float RifleAimBlendSpeed = 14.f;
constexpr double MaxRifleAimCorrection = FMath::DegreesToRadians(50.0);

FTransform GetComponentBoneTransform(const FCompactPose& Pose, FCompactPoseBoneIndex Bone)
{
	FTransform Transform = Pose[Bone];
	for (FCompactPoseBoneIndex Parent = Pose.GetParentBoneIndex(Bone); Parent.IsValid(); Parent = Pose.GetParentBoneIndex(Parent))
	{
		Transform *= Pose[Parent];
	}
	return Transform;
}

class FTBAnimInstanceProxy final : public FAnimInstanceProxy
{
public:
	explicit FTBAnimInstanceProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

protected:
	virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
	{
		FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);

		const ATBMonster* Hunter = Cast<ATBMonster>(Instance->TryGetPawnOwner());
		const USkeletalMeshComponent* Mesh = Instance->GetSkelMeshComponent();
		if (!Hunter || Hunter->GetMonsterBehavior() != ETBMonsterBehavior::Hunter || !Mesh
			|| Hunter->GetHunterCombatComponent()->GetPhase() != ETBHunterPhase::Rifle)
		{
			AimWeight = 0.f;
			return;
		}

		const bool bHasAim = Hunter->GetHunterCombatComponent()->TryGetRifleAimLocation(AimWorldLocation);
		AimWeight = FMath::FInterpTo(AimWeight, bHasAim ? 1.f : 0.f, DeltaSeconds, RifleAimBlendSpeed);
		if (AimWeight <= UE_KINDA_SMALL_NUMBER) return;

		SpineIndex = Mesh->GetBoneIndex(UpperSpineBone);
		MuzzleBoneIndex = Mesh->GetBoneIndex(Mesh->GetSocketBoneName(RifleMuzzleSocket));
		if (SpineIndex == INDEX_NONE || MuzzleBoneIndex == INDEX_NONE || !Mesh->DoesSocketExist(RifleMuzzleSocket))
		{
			AimWeight = 0.f;
			return;
		}
		MuzzleInBoneSpace = Mesh->GetSocketTransform(RifleMuzzleSocket, RTS_ParentBoneSpace);
	}

	virtual bool Evaluate_WithRoot(FPoseContext& Output, FAnimNode_Base* InRootNode) override
	{
		if (AimWeight <= UE_KINDA_SMALL_NUMBER) return false;
		EvaluateAnimationNode_WithRoot(Output, InRootNode);

		const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
		const FCompactPoseBoneIndex Spine = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(SpineIndex));
		const FCompactPoseBoneIndex Muzzle = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MuzzleBoneIndex));
		if (!Spine.IsValid() || !Muzzle.IsValid()) return true;

		FCompactPoseBoneIndex Ancestor = Muzzle;
		while (Ancestor.IsValid() && Ancestor != Spine) Ancestor = Output.Pose.GetParentBoneIndex(Ancestor);
		if (Ancestor != Spine) return true;

		const FTransform SpineInComponent = GetComponentBoneTransform(Output.Pose, Spine);
		const FTransform MuzzleInComponent = MuzzleInBoneSpace * GetComponentBoneTransform(Output.Pose, Muzzle);
		const FVector MuzzleOffset = MuzzleInComponent.GetLocation() - SpineInComponent.GetLocation();
		const FVector TargetOffset = GetComponentTransform().InverseTransformPosition(AimWorldLocation) - SpineInComponent.GetLocation();
		// 총구 소켓의 -X가 유효한 총열 전방
		const FVector BarrelForward = -MuzzleInComponent.GetUnitAxis(EAxis::X);
		const double Projection = FVector::DotProduct(MuzzleOffset, BarrelForward);
		const double Discriminant = FMath::Square(Projection) + TargetOffset.SizeSquared() - MuzzleOffset.SizeSquared();
		if (Discriminant <= 0.0) return true;

		// 회전축부터 표적까지의 거리와 같은 지점을 총구 직선 위에서 계산
		const double DistanceAlongBarrel = FMath::Sqrt(Discriminant) - Projection;
		if (DistanceAlongBarrel <= 0.0) return true;
		const FVector BarrelPoint = MuzzleOffset + BarrelForward * DistanceAlongBarrel;
		const FQuat Correction = FQuat::FindBetweenVectors(BarrelPoint, TargetOffset);
		const double Angle = Correction.GetAngle();
		const double Weight = AimWeight * FMath::Min(1.0, MaxRifleAimCorrection / FMath::Max(Angle, UE_SMALL_NUMBER));
		const FQuat ComponentDelta = FQuat::Slerp(FQuat::Identity, Correction, Weight);

		const FCompactPoseBoneIndex Parent = Output.Pose.GetParentBoneIndex(Spine);
		const FQuat ParentRotation = Parent.IsValid() ? GetComponentBoneTransform(Output.Pose, Parent).GetRotation() : FQuat::Identity;
		FTransform& SpineLocal = Output.Pose[Spine];
		SpineLocal.SetRotation((ParentRotation.Inverse() * ComponentDelta * ParentRotation * SpineLocal.GetRotation()).GetNormalized());
		return true;
	}

private:
	FVector AimWorldLocation = FVector::ZeroVector;
	FTransform MuzzleInBoneSpace = FTransform::Identity;
	int32 SpineIndex = INDEX_NONE;
	int32 MuzzleBoneIndex = INDEX_NONE;
	float AimWeight = 0.f;
};
}

FAnimInstanceProxy* UTBAnimInstance::CreateAnimInstanceProxy()
{
	return new FTBAnimInstanceProxy(this);
}

void UTBAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete InProxy;
}

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
