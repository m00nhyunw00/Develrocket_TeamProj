#include "Animation/Monster/TBAnimNotify_HunterSwordDash.h"

#include "AbilitySystem/Abilities/Monster/VampireHunter/TBGameplayAbility_VampireHunter_SwordDash.h"
#include "AbilitySystemComponent.h"
#include "Character/Monster/TBMonster.h"
#include "Components/SkeletalMeshComponent.h"

UTBAnimNotify_HunterSwordDash::UTBAnimNotify_HunterSwordDash()
{
	bIsNativeBranchingPoint = true;
}

void UTBAnimNotify_HunterSwordDash::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	const ATBMonster* Hunter = MeshComp ? Cast<ATBMonster>(MeshComp->GetOwner()) : nullptr;
	if (!IsValid(Hunter) || !Hunter->HasAuthority() || Hunter->GetMesh() != MeshComp)
	{
		return;
	}

	const UAbilitySystemComponent* ASC = Hunter->GetAbilitySystemComponent();
	auto* Ability = ASC ? Cast<UTBGameplayAbility_VampireHunter_SwordDash>(ASC->GetAnimatingAbility()) : nullptr;
	if (Ability)
	{
		Ability->DashFromMontageNotify(Animation, *this);
	}
}
