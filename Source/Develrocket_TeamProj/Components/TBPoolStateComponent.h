#pragma once

#include "Components/ActorComponent.h"
#include "TBPoolStateComponent.generated.h"

USTRUCT()
struct FTBPoolActivationState
{
	GENERATED_BODY()

	UPROPERTY()
	bool bActive = true;

	UPROPERTY()
	uint32 Generation = 0;
};

// Created by the spawn manager on pooled actors, including Blueprint-only actors.
UCLASS()
class UTBPoolStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTBPoolStateComponent();
	void SetPoolActive(bool bActive);
	bool IsPoolActive() const { return PoolState.bActive; }
	uint32 GetPoolGeneration() const { return PoolState.Generation; }
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_PoolState(FTBPoolActivationState PreviousState);

	void ApplyPoolState();

	UPROPERTY(ReplicatedUsing=OnRep_PoolState)
	FTBPoolActivationState PoolState;

	bool bAppliedInactive = false;
	bool bRestoreCollision = false;
	bool bRestoreActorTick = false;
	bool bRestoreMovementTick = false;
};
