#include "Animation/Monster/TBAnimNotify_MonsterMelee.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/Monster/TBGameplayAbility_Monster_Melee.h"
#include "Character/Monster/TBMonster.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
	UTBGameplayAbility_Monster_Melee* GetActiveMelee(USkeletalMeshComponent* MeshComp)
	{
		ATBMonster* Monster = MeshComp ? Cast<ATBMonster>(MeshComp->GetOwner()) : nullptr;
		if (!IsValid(Monster) || !Monster->HasAuthority() || Monster->GetMesh() != MeshComp) return nullptr;
		UAbilitySystemComponent* ASC = Monster->GetAbilitySystemComponent();
		const TSubclassOf<UGameplayAbility> AbilityClass = Monster->GetMeleeAbilityClass();
		FGameplayAbilitySpec* Spec = ASC && AbilityClass ? ASC->FindAbilitySpecFromClass(AbilityClass) : nullptr;
		return Spec && Spec->IsActive() ? Cast<UTBGameplayAbility_Monster_Melee>(Spec->GetPrimaryInstance()) : nullptr;
	}
}

void UTBAnimNotifyState_MonsterMelee::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (UTBGameplayAbility_Monster_Melee* Ability = GetActiveMelee(MeshComp)) Ability->BeginHitWindow();
}

void UTBAnimNotifyState_MonsterMelee::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	if (UTBGameplayAbility_Monster_Melee* Ability = GetActiveMelee(MeshComp)) Ability->EndHitWindow();
}
