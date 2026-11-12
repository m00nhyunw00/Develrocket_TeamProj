#include "ANS_Ratote.h"

#include "Animation/ANS_Ratote.h"

#include "Components/TBRotationComponent.h"

void UANS_Ratote::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
											  float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	
	if (UTBRotationComponent* RotationComponent = MeshComp->GetOwner()->GetComponentByClass<UTBRotationComponent>())
	{
		RotationComponent->ToggleShouldRotate(true);
	}
}

void UANS_Ratote::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	
	if (UTBRotationComponent* RotationComponent = MeshComp->GetOwner()->GetComponentByClass<UTBRotationComponent>())
	{
		RotationComponent->ToggleShouldRotate(false);
	}
}