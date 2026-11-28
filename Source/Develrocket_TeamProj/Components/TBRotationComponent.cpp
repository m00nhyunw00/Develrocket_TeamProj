#include "Components/TBRotationComponent.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/Monster/TBMonster.h"
#include "Character/Player/TBPlayer.h"
#include "Components/TBHunterCombatComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"

UTBRotationComponent::UTBRotationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

}

void UTBRotationComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const ATBMonster* Monster = Cast<ATBMonster>(GetOwner()); Monster && Monster->GetMonsterBehavior() == ETBMonsterBehavior::Hunter)
	{
		AddTickPrerequisiteComponent(Monster->GetCharacterMovement());
	}
}

bool UTBRotationComponent::IsTrackingHunterTarget() const
{
	const ATBMonster* Monster = Cast<ATBMonster>(GetOwner());
	return Monster && Monster->GetMonsterBehavior() == ETBMonsterBehavior::Hunter && GetFacingTarget();
}

AActor* UTBRotationComponent::GetFacingTarget() const
{
	const AActor* Owner = GetOwner();
	const ATBMonster* Monster = Cast<ATBMonster>(Owner);
	const bool bIsHunter = Monster && Monster->GetMonsterBehavior() == ETBMonsterBehavior::Hunter;
	if (!IsValid(Owner) || !Owner->HasAuthority() || (!bIsHunter && !bShouldRotate)
		|| (bIsHunter && Monster->IsDead()))
	{
		return nullptr;
	}

	const APawn* Pawn = Cast<APawn>(Owner);
	const AAIController* AIController = Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr;
	const UBlackboardComponent* Blackboard = AIController ? AIController->GetBlackboardComponent() : nullptr;
	static const FName TargetActorKey(TEXT("TargetActor"));
	AActor* Target = Blackboard ? Cast<AActor>(Blackboard->GetValueAsObject(TargetActorKey)) : nullptr;
	if (!IsValid(Target) || Target == Owner || Target->GetWorld() != Owner->GetWorld())
	{
		return nullptr;
	}

	if (bIsHunter)
	{
		const ATBPlayer* TargetPlayer = Cast<ATBPlayer>(Target);
		const ATBMonster* TargetMonster = Cast<ATBMonster>(Target);
		if ((TargetPlayer && TargetPlayer->IsDead()) || (TargetMonster && TargetMonster->IsDead())) return nullptr;
	}
	return Target;
}

void UTBRotationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* FacingTarget = GetFacingTarget();
	AActor* Owner = GetOwner();
	if (ATBMonster* Monster = Cast<ATBMonster>(Owner); Monster && Monster->HasAuthority()
		&& Monster->GetMonsterBehavior() == ETBMonsterBehavior::Hunter)
	{
		UTBHunterCombatComponent* Combat = Monster->GetHunterCombatComponent();
		Combat->SetRifleAimTarget(Combat->GetPhase() == ETBHunterPhase::Rifle ? FacingTarget : nullptr);
	}
	if (!FacingTarget) return;

	// 대상 방향으로 수평 회전
	FVector TargetOffset = FacingTarget->GetActorLocation() - Owner->GetActorLocation();
	TargetOffset.Z = 0.0;
	if (TargetOffset.IsNearlyZero()) return;

	// 현재 방향에서 목표 방향까지 일정한 속도로 회전
	const FRotator NewRotation = FMath::RInterpConstantTo(Owner->GetActorRotation(), TargetOffset.Rotation(), DeltaTime, RotationSpeed);

	// 수평 회전만 적용
	Owner->SetActorRotation(FRotator(0.f, NewRotation.Yaw, 0.f));
}
