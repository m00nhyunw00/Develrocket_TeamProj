#include "Animation/ANS_TBMeleeTrace.h"

#include "Components/SkeletalMeshComponent.h"
#include "Interface/TBCombatInterface.h"

void UANS_TBMeleeTrace::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(
		MeshComp, Animation, TotalDuration, EventReference);

	if (!IsValid(MeshComp))
	{
		return;
	}

	if (ITBCombatInterface* Combat =
		Cast<ITBCombatInterface>(MeshComp->GetOwner()))
	{
		Combat->BeginMeleeTrace(MeshComp);
	}
}

void UANS_TBMeleeTrace::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!IsValid(MeshComp))
	{
		return;
	}

	if (ITBCombatInterface* Combat =
		Cast<ITBCombatInterface>(MeshComp->GetOwner()))
	{
		Combat->EndMeleeTrace(MeshComp);
	}
}