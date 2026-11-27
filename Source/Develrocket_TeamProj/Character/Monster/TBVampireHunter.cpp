#include "Character/Monster/TBVampireHunter.h"

#include "AbilitySystem/Abilities/Monster/VampireHunter/TBGameplayAbility_VampireHunter_Dodge.h"
#include "AbilitySystemComponent.h"
#include "AI/AIControler/Monster/TBHunterAIController.h"
#include "Character/Monster/TBHunterAnimationData.h"

ATBVampireHunter::ATBVampireHunter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MonsterBehavior = ETBMonsterBehavior::Hunter;
	AIControllerClass = ATBHunterAIController::StaticClass();
}

const UTBHunterAnimationData* ATBVampireHunter::GetPhaseAnimationData(bool bSword) const
{
	return bSword ? SwordAnimationData.Get() : RifleAnimationData.Get();
}

bool ATBVampireHunter::TryDodgeRangedAttack()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(UTBGameplayAbility_VampireHunter_Dodge::StaticClass());
	const FGameplayAbilitySpecHandle Handle = Spec ? Spec->Handle : FGameplayAbilitySpecHandle();
	if (!Handle.IsValid() || !ASC->TryActivateAbility(Handle, false))
	{
		return false;
	}
	const FGameplayAbilitySpec* ActiveSpec = ASC->FindAbilitySpecFromHandle(Handle);
	return ActiveSpec && ActiveSpec->IsActive();
}
