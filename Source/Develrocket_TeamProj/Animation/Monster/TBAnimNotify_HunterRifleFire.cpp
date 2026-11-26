#include "Animation/Monster/TBAnimNotify_HunterRifleFire.h"

#include "AbilitySystem/Abilities/Monster/VampireHunter/TBGameplayAbility_VampireHunter_RifleSkills.h"
#include "AbilitySystemComponent.h"
#include "Character/Monster/TBMonster.h"
#include "Components/SkeletalMeshComponent.h"

UTBAnimNotify_HunterRifleFire::UTBAnimNotify_HunterRifleFire()
{
	bIsNativeBranchingPoint = true;
}

void UTBAnimNotify_HunterRifleFire::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	const ATBMonster* Hunter = MeshComp ? Cast<ATBMonster>(MeshComp->GetOwner()) : nullptr;
	if (!IsValid(Hunter) || !Hunter->HasAuthority() || Hunter->GetMesh() != MeshComp)
	{
		return;
	}

	const UAbilitySystemComponent* ASC = Hunter->GetAbilitySystemComponent();
	const auto* Ability = ASC ? Cast<UTBGameplayAbility_VampireHunter_NotifyShot>(ASC->GetAnimatingAbility()) : nullptr;
	if (Ability)
	{
		Ability->FireFromMontageNotify(Animation);
	}
}
