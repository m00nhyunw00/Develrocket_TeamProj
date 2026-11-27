#include "Components/TBRotationComponent.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"

UTBRotationComponent::UTBRotationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

}

void UTBRotationComponent::BeginPlay()
{
	Super::BeginPlay();

}

void UTBRotationComponent::SetAbilityFacingTarget(AActor* Target)
{
	if (Target)
	{
		AbilityFacingTarget.Emplace(Target);
	}
	else
	{
		AbilityFacingTarget.Reset();
	}
}

void UTBRotationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (!bShouldRotate || !IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}

	const APawn* Pawn = Cast<APawn>(Owner);
	const AAIController* AIController = Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr;
	const UBlackboardComponent* Blackboard = AIController ? AIController->GetBlackboardComponent() : nullptr;
	const AActor* FacingTarget = AbilityFacingTarget.IsSet() ? AbilityFacingTarget.GetValue().Get()
		: (Blackboard ? Cast<AActor>(Blackboard->GetValueAsObject(TEXT("TargetActor"))) : nullptr);
	if (!IsValid(FacingTarget) || FacingTarget == Owner || FacingTarget->GetWorld() != Owner->GetWorld())
	{
		return;
	}

	// 대상 방향으로 수평 회전
	FVector TargetOffset = FacingTarget->GetActorLocation() - Owner->GetActorLocation();
	TargetOffset.Z = 0.0;
	if (TargetOffset.IsNearlyZero()) return;

	// 현재 방향에서 목표 방향까지 일정한 속도로 회전
	const FRotator NewRotation = FMath::RInterpConstantTo(Owner->GetActorRotation(), TargetOffset.Rotation(), DeltaTime, RotationSpeed);

	// 수평 회전만 적용
	Owner->SetActorRotation(FRotator(0.f, NewRotation.Yaw, 0.f));
}
