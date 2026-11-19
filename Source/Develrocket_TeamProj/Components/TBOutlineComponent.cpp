#include "Components/TBOutlineComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Rendering/TBOutlineSubsystem.h"

void UTBOutlineComponent::SetOutlineEnabled(bool bEnabled)
{
	bOutlineEnabled = bEnabled;
	RefreshOutline();
}

void UTBOutlineComponent::SetOutlineStyle(const FTBOutlineStyle& NewStyle)
{
	Style = NewStyle;
	RefreshOutline();
}

void UTBOutlineComponent::OnRegister()
{
	Super::OnRegister();
	RefreshOutline();
}

void UTBOutlineComponent::OnUnregister()
{
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

	if (bOutlineEnabled && IsValid(Mesh) && (Mesh->IsA<USkeletalMeshComponent>() || Mesh->IsA<UStaticMeshComponent>()))
	{
		Subsystem->SetOutline(this, Mesh, Style);
	}
	else
	{
		Subsystem->RemoveOutline(this);
	}
}
