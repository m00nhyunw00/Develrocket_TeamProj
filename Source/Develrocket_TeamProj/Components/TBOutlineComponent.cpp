#include "Components/TBOutlineComponent.h"

#include "Character/TBCharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Rendering/TBOutlineSubsystem.h"
#include "TimerManager.h"

void UTBOutlineComponent::SetOutlineSuppressed(bool bSuppressed)
{
	if (bOutlineSuppressed == bSuppressed)
	{
		return;
	}

	bOutlineSuppressed = bSuppressed;
	if (bOutlineSuppressed) ClearBloodPulseReveal();
	RefreshOutline();
}

bool UTBOutlineComponent::CanRevealWithBloodPulse() const
{
	return !bOutlineSuppressed
		&& (bDetectableByBloodPulse || (GetOwner() && GetOwner()->IsA<ATBCharacterBase>()));
}

void UTBOutlineComponent::RevealWithBloodPulse(float Duration)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer || !CanRevealWithBloodPulse() || Duration <= 0.0f)
	{
		return;
	}

	World->GetTimerManager().SetTimer(BloodPulseRevealTimer, this, &ThisClass::ClearBloodPulseReveal, Duration, false);
	RefreshOutline();
}

void UTBOutlineComponent::ClearBloodPulseReveal()
{
	if (UWorld* World = GetWorld(); World && BloodPulseRevealTimer.IsValid())
	{
		World->GetTimerManager().ClearTimer(BloodPulseRevealTimer);
		RefreshOutline();
	}
}

void UTBOutlineComponent::SetOutlineStyle(const FTBOutlineStyle& NewStyle)
{
	Style = NewStyle;
	RefreshOutline();
}

void UTBOutlineComponent::SetOutlineColor(float Red, float Green, float Blue)
{
	if (!FMath::IsFinite(Red) || !FMath::IsFinite(Green) || !FMath::IsFinite(Blue))
	{
		return;
	}

	Style.Color = FLinearColor(FMath::Max(0.0f, Red), FMath::Max(0.0f, Green), FMath::Max(0.0f, Blue), Style.Color.A);
	RefreshOutline();
}

void UTBOutlineComponent::OnRegister()
{
	Super::OnRegister();
	RefreshOutline();
}

void UTBOutlineComponent::OnUnregister()
{
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(BloodPulseRevealTimer);
	if (UTBOutlineSubsystem* Subsystem = UWorld::GetSubsystem<UTBOutlineSubsystem>(GetWorld()))
	{
		Subsystem->RemoveOutline(this);
	}
	Super::OnUnregister();
}

#if WITH_EDITOR
void UTBOutlineComponent::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	RefreshOutline();
	Super::PostEditChangeProperty(Event);
}
#endif

void UTBOutlineComponent::RefreshOutline()
{
	AActor* Owner = GetOwner();
	if (!IsRegistered() || !IsValid(Owner))
	{
		return;
	}

	UTBOutlineSubsystem* Subsystem = UWorld::GetSubsystem<UTBOutlineSubsystem>(GetWorld());
	if (!Subsystem)
	{
		return;
	}

	if (!BloodPulseRevealTimer.IsValid() || !CanRevealWithBloodPulse())
	{
		Subsystem->RemoveOutline(this);
		return;
	}

	UPrimitiveComponent* Mesh = Cast<UPrimitiveComponent>(TargetMesh.GetComponent(Owner));
	if (TargetMesh == FComponentReference{} && !Cast<USkeletalMeshComponent>(Mesh))
	{
		if (USkeletalMeshComponent* SkeletalMesh = Owner->FindComponentByClass<USkeletalMeshComponent>())
		{
			Mesh = SkeletalMesh;
		}
		else if (!Cast<UStaticMeshComponent>(Mesh))
		{
			Mesh = Owner->FindComponentByClass<UStaticMeshComponent>();
		}
	}

	if (IsValid(Mesh) && (Mesh->IsA<USkeletalMeshComponent>() || Mesh->IsA<UStaticMeshComponent>()))
	{
		Subsystem->SetOutline(this, Mesh, Style);
	}
	else
	{
		Subsystem->RemoveOutline(this);
	}
}
