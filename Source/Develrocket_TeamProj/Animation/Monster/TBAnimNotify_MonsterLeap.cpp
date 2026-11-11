#include "Animation/Monster/TBAnimNotify_MonsterLeap.h"

#include "AbilitySystem/Abilities/Monster/TBGameplayAbility_Monster_Leap.h"
#include "AbilitySystemComponent.h"
#include "Character/Monster/TBMonster.h"
#include "Components/SkeletalMeshComponent.h"

void UTBAnimNotify_MonsterLeap::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	ATBMonster* Monster = MeshComp ? Cast<ATBMonster>(MeshComp->GetOwner()) : nullptr;
	if (!IsValid(Monster) || !Monster->HasAuthority() || Monster->GetMesh() != MeshComp) return;

	// 서버에서 몽타주를 재생 중인 도약 어빌리티에 이륙 전달
	UAbilitySystemComponent* ASC = Monster->GetAbilitySystemComponent();
	UTBGameplayAbility_Monster_Leap* Leap = ASC ? Cast<UTBGameplayAbility_Monster_Leap>(ASC->GetAnimatingAbility()) : nullptr;
	if (Leap) Leap->LaunchLeap();
}
