#pragma once

#include "Components/ActorComponent.h"
#include "Character/Monster/TBHunterAnimationData.h"
#include "TBHunterCombatComponent.generated.h"

class ATBMonster;
class UAnimInstance;
class UAnimMontage;
class UAnimSequence;
class USkeletalMesh;

UENUM(BlueprintType)
enum class ETBHunterPhase : uint8 { Rifle, DrawingSword, Sword };

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
	void ResetPhase();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UFUNCTION()
	void OnRep_CombatState();
	bool IsAtTransitionHealth() const;
	void SetCombatState(ETBHunterPhase Phase, ETBHunterSwordAnimation Animation);
	void UpdateSwordVisual();
	void LoadPhaseAnimations(const UTBHunterAnimationData* RifleData, const UTBHunterAnimationData* SwordData);
	double GetCombatTime() const;

	UPROPERTY(EditDefaultsOnly, Category = "Hunter|Phase", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float SwordPhaseHealthFraction = 0.5f;

	UPROPERTY(ReplicatedUsing = OnRep_CombatState)
	FTBHunterCombatState CombatState;

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
