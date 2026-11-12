#pragma once

#include "CoreMinimal.h"
#include "AI/TBBaseAIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "TBMonsterAIController.generated.h"

class UAISenseConfig_Sight;
class ATBMonster;
class UAbilitySystemComponent;
struct FGameplayTag;

UCLASS()
class DEVELROCKET_TEAMPROJ_API ATBMonsterAIController : public ATBBaseAIController
{
	GENERATED_BODY()
	
public:
	 ATBMonsterAIController();

	void InvestigateDamageSource(const AActor* DamageSource);
	void TryStartPendingInvestigation();
	void ReactivateFromPool();
	
protected:
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
	bool bDrawSightDebug = true;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Perception|Debug", meta = (ClampMin = "0.05", Units = "s", EditCondition = "bDrawSightDebug"))
	float SightDebugInterval = 0.2f;

private:
	void DrawSightDebug();
	void FinishInvestigation(bool bStopMove);
	TOptional<FVector> PendingInvestigationLocation;
	FAIRequestID InvestigationRequestID;
	bool bPausedBrainForInvestigation = false;
	FTimerHandle SightDebugTimer;
};
