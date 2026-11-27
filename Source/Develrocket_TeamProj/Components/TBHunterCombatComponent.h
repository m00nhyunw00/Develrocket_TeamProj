#pragma once

#include "Components/ActorComponent.h"
#include "Character/Monster/TBHunterAnimationData.h"
#include "Engine/NetSerialization.h"
#include "TimerManager.h"
#include "TBHunterCombatComponent.generated.h"

class ATBMonster;
class UAnimInstance;
class UAnimMontage;
class UAnimSequence;
class USkeletalMesh;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UPoseableMeshComponent;

UENUM(BlueprintType)
enum class ETBHunterPhase : uint8 { Rifle, DrawingSword, Sword };

UENUM()
enum class ETBHunterRifleTracer : uint8 { Basic, Traveling, Charged };

USTRUCT()
struct FTBHunterRifleAimLock
{
	GENERATED_BODY()

	UPROPERTY()
	FVector_NetQuantize10 Location = FVector::ZeroVector;

	UPROPERTY()
	double EndTime = 0.0;
};

USTRUCT()
struct FTBHunterCombatState
{
	GENERATED_BODY()

	UPROPERTY()
	ETBHunterPhase Phase = ETBHunterPhase::Rifle;

	UPROPERTY()
	ETBHunterSwordAnimation Animation = ETBHunterSwordAnimation::Idle;

	UPROPERTY()
	double AnimationStartTime = 0.0;
};

USTRUCT()
struct FTBHunterChargeVisual
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<AActor> Target;

	UPROPERTY()
	float Duration = 0.f;

	UPROPERTY()
	double StartTime = 0.0;
};

UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBHunterCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTBHunterCombatComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	ETBHunterPhase GetPhase() const { return CombatState.Phase; }
	bool CanStartRifleAttack() const;
	bool TryStartSwordAttack(AActor& Target);
	void SetSwordAnimation(ETBHunterSwordAnimation Animation);
	float GetSwordAnimationDuration(ETBHunterSwordAnimation Animation) const;
	float GetSwordHitFraction(ETBHunterSwordAnimation Animation) const;
	void DeferSwordAttackRetry();
	UAnimMontage* GetRifleAttackMontage() const { return RifleAttackMontage.Get(); }
	UAnimMontage* GetRifleNotifyShotMontage() const { return RifleNotifyShotMontage.Get(); }
	void ResetPhase();
	void BeginRifleChargeVisual(AActor* Target, float Duration);
	void EndRifleChargeVisual();
	void SetRifleAimTarget(AActor* Target);
	bool TryGetRifleAimLocation(FVector& OutLocation) const;
	void SetRifleAimLock(const FVector& Location, float Duration);
	void ClearRifleAimLock();
	static constexpr float BasicTracerDuration = 0.07f;
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayRifleTracer(FVector_NetQuantize10 TraceStart, FVector_NetQuantize10 Impact, ETBHunterRifleTracer TracerType);
	UFUNCTION(NetMulticast, Reliable)
	void MulticastStartMotionTrail(float Duration);
	UFUNCTION(NetMulticast, Reliable)
	void MulticastStopMotionTrail();
	static bool TryGetRifleAimRay(const ATBMonster& Hunter, FName MuzzleSocket, const FVector& AimLocation,
		float Range, FVector& OutStart, FVector& OutEnd);
	static bool TraceRifleShot(const ATBMonster& Hunter, const FVector& Start, const FVector& End, FHitResult& OutHit);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	struct FMotionTrailGhost
	{
		TWeakObjectPtr<UPoseableMeshComponent> Mesh;
		TWeakObjectPtr<UMaterialInstanceDynamic> Material;
		double SpawnedAt = 0.0;
	};

	struct FRifleTracer
	{
		TWeakObjectPtr<UStaticMeshComponent> Mesh;
		FVector Start;
		FVector Direction;
		double Length;
		double StartTime;
		double InitialHeadDistance;
		double Speed;
		double TailLength;
		double Radius;
	};

	UFUNCTION()
	void OnRep_CombatState();
	UFUNCTION()
	void OnRep_ChargeVisual();
	void UpdateChargeBeam();
	void ClearChargeBeamPoseUpdate();
	void UpdateRifleTracers();
	void UpdateMotionTrail();
	void SpawnMotionTrailGhost(double Now);
	UStaticMeshComponent* CreateRifleBeam(UMaterialInterface* Material);
	bool IsAtTransitionHealth() const;
	void SetCombatState(ETBHunterPhase Phase, ETBHunterSwordAnimation Animation);
	void UpdateSwordVisual();
	void LoadPhaseAnimations(const UTBHunterAnimationData* RifleData, const UTBHunterAnimationData* SwordData);
	double GetCombatTime() const;

	UPROPERTY(EditDefaultsOnly, Category = "Hunter|Phase", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float SwordPhaseHealthFraction = 0.5f;

	UPROPERTY(ReplicatedUsing = OnRep_CombatState)
	FTBHunterCombatState CombatState;

	UPROPERTY(ReplicatedUsing = OnRep_ChargeVisual)
	FTBHunterChargeVisual ChargeVisual;

	UPROPERTY(Replicated)
	TObjectPtr<AActor> RifleAimTarget;

	UPROPERTY(Replicated)
	FTBHunterRifleAimLock RifleAimLock;

	UPROPERTY()
	TObjectPtr<UStaticMesh> ChargeBeamMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> ChargeBeamMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> RifleTracerMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> ChargeBeam;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ChargeBeamMaterialInstance;

	FTimerHandle ChargeBeamTimer;
	FDelegateHandle ChargeBeamPoseHandle;
	FTimerHandle RifleTracerTimer;
	TArray<FRifleTracer> RifleTracers;
	FTimerHandle MotionTrailTimer;
	TArray<FMotionTrailGhost> MotionTrailGhosts;
	double MotionTrailUntil = 0.0;
	double NextMotionTrailSample = 0.0;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMesh> LoadedRifleMesh;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMesh> LoadedSwordMesh;

	UPROPERTY(Transient)
	TSubclassOf<UAnimInstance> RifleAnimClass;

	UPROPERTY(Transient)
	TSubclassOf<UAnimInstance> SwordAnimClass;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> RifleAttackMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Hunter|Animation")
	TObjectPtr<UAnimMontage> RifleNotifyShotMontage;

	UPROPERTY(Transient)
	TMap<ETBHunterSwordAnimation, TObjectPtr<UAnimSequence>> SwordActions;

	TMap<ETBHunterSwordAnimation, float> SwordHitFractions;
	TWeakObjectPtr<UAnimMontage> PlayingSwordMontage;
	FName SwordFullBodySlot = TEXT("DefaultSlot");
	TOptional<ETBHunterSwordAnimation> PlayingAnimation;
	double PlayingAnimationStartTime = 0.0;
	double NextSwordAttackTime = 0.0;
	bool bTransitionRequested = false;
	bool bSwordAnimationMode = false;
};
