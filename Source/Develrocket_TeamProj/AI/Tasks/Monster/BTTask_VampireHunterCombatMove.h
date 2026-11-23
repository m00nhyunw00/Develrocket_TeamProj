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
	bool TryLocalMove(AAIController& Controller, const FVector& Origin, const FVector& Goal, const FVector& TargetLocation, double MinimumRadius);
	bool StartMove(AAIController& Controller, FAIMoveRequest& Request);
	void StopOwnedMove(AAIController& Controller);

	TWeakObjectPtr<ATBMonster> ActiveMonster;
	TWeakObjectPtr<AActor> FocusTarget;
	TWeakObjectPtr<AActor> PreviousFocusActor;
	FVector PreviousFocalPoint = FAISystem::InvalidLocation;
	FVector LastTargetLocation = FVector::ZeroVector;
	FAIRequestID MoveId;
	EMovementMode MovementMode = EMovementMode::Chase;
	float TimeUntilUpdate = 0.0f;
	float OrbitSide = 1.0f;
	bool bPreviousOrientToMovement = false;
	bool bPreviousUseControllerDesiredRotation = false;
	bool bPreviousUseControllerYaw = false;
};
