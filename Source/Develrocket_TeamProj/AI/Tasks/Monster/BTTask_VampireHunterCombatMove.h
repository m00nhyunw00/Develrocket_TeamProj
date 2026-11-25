#pragma once

#include "AITypes.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_VampireHunterCombatMove.generated.h"

class AAIController;
class ATBMonster;

UCLASS()
class DEVELROCKET_TEAMPROJ_API UBTTask_VampireHunterCombatMove : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_VampireHunterCombatMove();

protected:
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetActorKey;

	UPROPERTY(EditAnywhere, Category = "Combat Movement", meta = (ClampMin = "1.0", Units = "cm"))
	float OrbitStartDistance = 300.0f;

	UPROPERTY(EditAnywhere, Category = "Combat Movement", meta = (ClampMin = "1.0", Units = "cm"))
	float OrbitPreferredDistance = 245.0f;

	UPROPERTY(EditAnywhere, Category = "Combat Movement", meta = (ClampMin = "1.0", Units = "cm"))
	float RetreatStartDistance = 180.0f;

	UPROPERTY(EditAnywhere, Category = "Combat Movement", meta = (ClampMin = "1.0", Units = "cm"))
	float RetreatEndDistance = 230.0f;

private:
	enum class EMovementMode : uint8 { Chase, Orbit, Retreat };

	bool UpdateMovement(UBehaviorTreeComponent& OwnerComp);
	void SearchForFiringAngle(AAIController& Controller, const FVector& Origin);
	bool FindOrbitGoal(AAIController& Controller, const FVector& From, const FVector& TargetLocation, double MinimumRadius);
	bool FindLocalGoal(AAIController& Controller, const FVector& From, const FVector& Goal, const FVector& TargetLocation,
		double MinimumRadius, FVector& OutGoal) const;
	bool StartLocalMove(AAIController& Controller, const FVector& Goal);
	bool StartMove(AAIController& Controller, FAIMoveRequest& Request);
	void StopOwnedMove(AAIController& Controller);

	TWeakObjectPtr<ATBMonster> ActiveMonster;
	TWeakObjectPtr<AActor> FocusTarget;
	TWeakObjectPtr<AActor> PreviousFocusActor;
	FVector PreviousFocalPoint = FAISystem::InvalidLocation;
	FVector LastTargetLocation = FVector::ZeroVector;
	FVector ActiveOrbitGoal = FAISystem::InvalidLocation;
	FVector NextOrbitGoal = FAISystem::InvalidLocation;
	FVector OrbitPlanTargetLocation = FVector::ZeroVector;
	FVector LastVisibleTargetLocation = FAISystem::InvalidLocation;
	FAIRequestID MoveId;
	EMovementMode MovementMode = EMovementMode::Chase;
	EMovementMode ActiveMoveMode = EMovementMode::Chase;
	float TimeUntilUpdate = 0.0f;
	double NextOrbitPlanTime = 0.0;
	double NextSightSearchTime = 0.0;
	float OrbitSide = 1.0f;
	float NextOrbitSide = 1.0f;
	int32 OrbitLegsUntilSideChoice = 0;
	bool bPreviousOrientToMovement = false;
	bool bPreviousUseControllerDesiredRotation = false;
	bool bPreviousUseControllerYaw = false;
	bool bPreviousAllowStrafe = false;
	bool bHadLineOfSight = false;
};
