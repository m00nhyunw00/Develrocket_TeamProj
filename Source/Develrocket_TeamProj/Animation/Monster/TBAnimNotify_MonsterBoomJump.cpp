#include "Animation/Monster/TBAnimNotify_MonsterBoomJump.h"

#include "AbilitySystem/Abilities/Monster/TBGameplayAbility_Monster_Boom.h"
#include "AbilitySystemComponent.h"
#include "Character/Monster/TBMonster.h"
#include "Components/SkeletalMeshComponent.h"

void UTBAnimNotify_MonsterBoomJump::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	ATBMonster* Monster = MeshComp ? Cast<ATBMonster>(MeshComp->GetOwner()) : nullptr;
	if (!IsValid(Monster) || !Monster->HasAuthority() || Monster->GetMesh() != MeshComp) return;

	UAbilitySystemComponent* ASC = Monster->GetAbilitySystemComponent();
	auto* Boom = ASC ? Cast<UTBGameplayAbility_Monster_Boom>(ASC->GetAnimatingAbility()) : nullptr;
	if (Boom) Boom->JumpForward(Animation);
}
