#include "AbilitySystem/TBAbilitySystemComponent.h"

#include "Abilities/GameplayAbility.h"

void UTBAbilitySystemComponent::AddCharacterAbilities(const TArray<TSubclassOf<UGameplayAbility>>& StartupAbilities)
{
	// Ability 부여는 서버에서만 수행
	if (!IsOwnerActorAuthoritative())
	{
		return;
	}

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : StartupAbilities)
	{
		if (!AbilityClass)
		{
			continue;
		}

		// 기본 Ability의 동일 클래스 중복 부여 방지
		if (FindAbilitySpecFromClass(AbilityClass))
		{
			continue;
		}

		FGameplayAbilitySpec AbilitySpec(AbilityClass, 1, INDEX_NONE, GetOwnerActor());

		const FGameplayAbilitySpecHandle SpecHandle = GiveAbility(AbilitySpec);

		if (SpecHandle.IsValid())
		{
			StartupAbilityHandles.Add(SpecHandle);
		}
	}
}

void UTBAbilitySystemComponent::RemoveCharacterAbilities()
{
	if (!IsOwnerActorAuthoritative())
	{
		return;
	}

	// 제거 콜백과 부여 기록 분리
	const TArray<FGameplayAbilitySpecHandle> HandlesToRemove = StartupAbilityHandles;
	StartupAbilityHandles.Reset();

	for (const FGameplayAbilitySpecHandle& SpecHandle : HandlesToRemove)
	{
		if (FindAbilitySpecFromHandle(SpecHandle))
		{
			ClearAbility(SpecHandle);
		}
	}
}

bool UTBAbilitySystemComponent::TryActivateAbilityByTag(const FGameplayTag& AbilityTag)
{
	if (!AbilityTag.IsValid() || !GetAvatarActor())
	{
		return false;
	}

	const FGameplayTagContainer AbilityTags(AbilityTag);

	// ASC가 관리하는 Ability Spec에서 태그 검색 및 활성화
	return TryActivateAbilitiesByTag(AbilityTags);
}