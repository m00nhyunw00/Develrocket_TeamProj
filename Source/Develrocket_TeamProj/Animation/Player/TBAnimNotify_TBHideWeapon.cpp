#include "Animation/Player/TBAnimNotify_TBHideWeapon.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Character/Player/TBPlayer.h"

void UTBAnimNotify_TBHideWeapon::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	SetWeaponsHidden(MeshComp, true);
}

void UTBAnimNotify_TBHideWeapon::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	
	if (const ATBPlayer* Player = MeshComp ? Cast<ATBPlayer>(MeshComp->GetOwner()) : nullptr)
	{
		if (Player->IsDead())
		{
			return;
		}
	}
	SetWeaponsHidden(MeshComp, false);
}

void UTBAnimNotify_TBHideWeapon::SetWeaponsHidden(USkeletalMeshComponent* MeshComp, bool bHidden)
{
	if (!MeshComp)
	{
		return;
	}
	
	for (USceneComponent* Child : MeshComp -> GetAttachChildren())
	{
		if (UStaticMeshComponent* Weapon = Cast<UStaticMeshComponent>(Child))
		{
			Weapon->SetHiddenInGame(bHidden);
		}
	}
}

