#include "Animation/Monster/TBAnimNotify_MonsterMelee.h"

#include "Components/SkeletalMeshComponent.h"
#include "Interface/TBCombatInterface.h"

namespace
{
	ITBCombatInterface* GetCombatInterface(USkeletalMeshComponent* MeshComp)
	{
		return IsValid(MeshComp) ? Cast<ITBCombatInterface>(MeshComp->GetOwner()) : nullptr;
	}
}

void UTBAnimNotifyState_MonsterMelee::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (ITBCombatInterface* Combat = GetCombatInterface(MeshComp)) Combat->BeginMeleeTrace(MeshComp);
}

void UTBAnimNotifyState_MonsterMelee::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	if (ITBCombatInterface* Combat = GetCombatInterface(MeshComp)) Combat->EndMeleeTrace(MeshComp);
}

void UTBAnimNotify_MonsterMeleeStart::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	if (ITBCombatInterface* Combat = GetCombatInterface(MeshComp)) Combat->BeginMeleeTrace(MeshComp);
}

void UTBAnimNotify_MonsterMeleeEnd::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	if (ITBCombatInterface* Combat = GetCombatInterface(MeshComp)) Combat->EndMeleeTrace(MeshComp);
}
