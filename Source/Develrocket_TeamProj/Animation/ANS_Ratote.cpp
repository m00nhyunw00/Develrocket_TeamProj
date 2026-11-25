#include "ANS_Ratote.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/TBRotationComponent.h"
#include "GameFramework/Actor.h"

namespace
{
UTBRotationComponent* FindRotationComponent(USkeletalMeshComponent* MeshComp)
{
	if (!IsValid(MeshComp)) return nullptr;

	AActor* Owner = MeshComp->GetOwner();
	return IsValid(Owner) ? Owner->GetComponentByClass<UTBRotationComponent>() : nullptr;
}
}

void UANS_Ratote::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
											  float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (UTBRotationComponent* RotationComponent = FindRotationComponent(MeshComp))
	{
		RotationComponent->ToggleShouldRotate(true);
	}
}

void UANS_Ratote::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (UTBRotationComponent* RotationComponent = FindRotationComponent(MeshComp))
	{
		RotationComponent->ToggleShouldRotate(false);
	}
}
