#pragma once

#include "CoreMinimal.h"
#include "AI/TBBaseAIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "TBMonsterAIController.generated.h"

class UAISenseConfig_Sight;
class ATBMonster;
class UAbilitySystemComponent;
class UBehaviorTreeComponent;
class UBTTask_MoveTo;
struct FGameplayTag;

UCLASS()
class DEVELROCKET_TEAMPROJ_API ATBMonsterAIController : public ATBBaseAIController
{
	GENERATED_BODY()
	
public:
	 ATBMonsterAIController();

	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;
	virtual bool RunBehaviorTree(UBehaviorTree* BTAsset) override;
	virtual FPathFollowingRequestResult MoveTo(const FAIMoveRequest& MoveRequest, FNavPathSharedPtr* OutPath = nullptr) override;
	void RefreshCombatPursuit(UBehaviorTreeComponent& Tree, const UBTTask_MoveTo& MoveNode);
	bool WantsCombatRetreat(const AActor& Target) const;
	bool TryAnasimusBackstep(AActor& Target);
	bool IsAnasimusBackstepping() const { return AnasimusMoveID.IsValid() && PreviousBackstepSpeed.IsSet(); }
	void StopAnasimusBackstep();

	void InvestigateDamageSource(AActor* DamageSource);
	void TryStartPendingInvestigation();
	void PreserveTargetDuringStagger();
	void RefreshTargetAfterStagger();
	void DeactivateForPool();
	void ReactivateFromPool();
	
protected:
	virtual bool CanTargetActor(const AActor& Actor) const;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;
	
	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	UFUNCTION()
	void HandleTargetDestroyed(AActor* DestroyedActor);

	UFUNCTION()
	void HandleMonsterDied(ATBMonster* Monster);

	void UpdateTarget(AActor* ExcludedActor = nullptr, bool bMemoryExpired = false);
	void HandleTargetMemoryExpired();
	void HandleTargetDeadTagChanged(FGameplayTag Tag, int32 NewCount);
	void ClearTrackedTarget();
	void SetTrackedTarget(AActor* Target);
	TWeakObjectPtr<AActor> TrackedTarget;
	TWeakObjectPtr<UAbilitySystemComponent> TrackedTargetASC;
	FDelegateHandle TargetDeadTagHandle;
	FTimerHandle TargetMemoryTimer;
	
	UPROPERTY(VisibleAnywhere, Category = "AI|Perception")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;
	
	UPROPERTY(EditDefaultsOnly, Category = "AI|Perception")
	float SightRadius = 1500.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "AI|Perception")
	float LoseSightRadius = 1800.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "AI|Perception")
	float PeripheralVisionAngle = 60.f;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Perception", meta = (ClampMin = "0.0", Units = "s"))
	float LostSightMemorySeconds = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Perception|Debug")
	bool bDrawSightDebug = false;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Perception|Debug", meta = (ClampMin = "0.05", Units = "s", EditCondition = "bDrawSightDebug"))
	float SightDebugInterval = 0.2f;

private:
	bool ShouldUseCombatWaitingMove(const ATBMonster& Monster, const AActor& Target) const;
	FPathFollowingRequestResult MoveAroundCombatTarget(const FAIMoveRequest& MoveRequest, ATBMonster& Monster,
		AActor& Target, FNavPathSharedPtr* OutPath);
	void FinishCombatWaitingMove(bool bAbortMove);
	void ClearCombatWaitingSlot();
	FPathFollowingRequestResult MoveAnasimusTowardTarget(const FAIMoveRequest& MoveRequest, AActor& Target,
		ATBMonster& Monster, FNavPathSharedPtr* OutPath);
	FPathFollowingRequestResult RequestAnasimusMove(FAIMoveRequest Request, AActor& Target, FNavPathSharedPtr* OutPath);
	void FinishAnasimusMove(bool bAbortMove);
	void DrawSightDebug();
	void FinishInvestigation(bool bStopMove);
	TOptional<FVector> PendingInvestigationLocation;
	FAIRequestID InvestigationRequestID;
	bool bPausedBrainForInvestigation = false;
	bool bPreserveTargetDuringStagger = false;
	bool bDamagePriorityTarget = false;
	bool bPoolSuspended = false;
	FTimerHandle SightDebugTimer;
	FAIRequestID CombatWaitingMoveID;
	FVector WaitingPlanTargetLocation = FVector::ZeroVector;
	double NextWaitingPlanTime = 0.0;
	int32 CombatWaitingSlot = INDEX_NONE;
	int32 WaitingOrbitDirection = 1;
	bool bCombatStandOff = false;
	bool bPreviousWaitingAllowStrafe = false;
	bool bPreviousWaitingOrientToMovement = false;
	bool bPreviousWaitingUseControllerRotation = false;
	bool bPreviousWaitingUseControllerYaw = false;
	double NextCombatRepositionTime = 0.0;
	FAIRequestID AnasimusMoveID;
	TOptional<float> PreviousBackstepSpeed;
	bool bPreviousAnasimusOrientToMovement = false;
	bool bPreviousAnasimusUseControllerRotation = false;
};
