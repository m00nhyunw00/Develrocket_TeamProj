#include "Components/TBPoolStateComponent.h"

#include "Components/TBOutlineComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Interface/TBPoolInterface.h"
#include "Net/UnrealNetwork.h"

UTBPoolStateComponent::UTBPoolStateComponent()
{
	SetIsReplicatedByDefault(true);
}

void UTBPoolStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UTBPoolStateComponent, PoolState);
}

void UTBPoolStateComponent::SetPoolActive(bool bActive)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority() || bActive == PoolState.bActive) return;

	Owner->FlushNetDormancy();
	PoolState.bActive = bActive;
	if (bActive) ++PoolState.Generation;
	ApplyPoolState();
	Owner->ForceNetUpdate();
}

void UTBPoolStateComponent::OnRep_PoolState(FTBPoolActivationState PreviousState)
{
	ApplyPoolState();
	AActor* Owner = GetOwner();
	if (PoolState.bActive && PoolState.Generation != PreviousState.Generation && IsValid(Owner)
		&& Owner->Implements<UTBPoolInterface>())
	{
		ITBPoolInterface::Execute_ClearHitImpactDecals(Owner);
	}
}

void UTBPoolStateComponent::ApplyPoolState()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner)) return;
	if (UTBOutlineComponent* Outline = Owner->FindComponentByClass<UTBOutlineComponent>())
	{
		Outline->ClearBloodPulseReveal();
	}
	ACharacter* Character = Cast<ACharacter>(Owner);
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;

	if (!PoolState.bActive)
	{
		if (!bAppliedInactive)
		{
			bRestoreCollision = Owner->GetActorEnableCollision();
			bRestoreActorTick = Owner->IsActorTickEnabled();
			bRestoreMovementTick = Movement && Movement->IsComponentTickEnabled();
			bAppliedInactive = true;
		}
		Owner->SetActorHiddenInGame(true);
		Owner->SetActorTickEnabled(false);
		if (Movement) Movement->SetComponentTickEnabled(false);
		Owner->SetActorEnableCollision(false);
	}
	else if (bAppliedInactive)
	{
		bAppliedInactive = false;
		Owner->SetActorHiddenInGame(false);
		Owner->SetActorTickEnabled(bRestoreActorTick);
		if (Movement) Movement->SetComponentTickEnabled(bRestoreMovementTick);
		Owner->SetActorEnableCollision(bRestoreCollision);
	}
}
