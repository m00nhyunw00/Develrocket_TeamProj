#include "Components/TBRotationComponent.h"

#include "Kismet/KismetMathLibrary.h"

UTBRotationComponent::UTBRotationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

}

void UTBRotationComponent::BeginPlay()
{
	Super::BeginPlay();

}

void UTBRotationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!TargetActor)
	{
		return;
	}

	if (!bShouldRotate)
	{
		return;
	}

	// 플레이어를 바라보는 목표 방향 계산
	const FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(GetOwner()->GetActorLocation(),TargetActor->GetActorLocation());

	// 현재 방향에서 목표 방향까지 일정한 속도로 회전
	const FRotator NewRotation = FMath::RInterpConstantTo(GetOwner()->GetActorRotation(),FRotator(0.f, LookAtRotation.Yaw, 0.f),DeltaTime,RotationSpeed);

	// 수평 회전만 적용
	GetOwner()->SetActorRotation(FRotator(0.f, NewRotation.Yaw, 0.f));
}