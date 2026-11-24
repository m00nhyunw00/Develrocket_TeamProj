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

void UTBAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	ABILITYLIST_SCOPE_LOCK();

	for (FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (!Spec.Ability || !Spec.Ability->GetAssetTags().HasTagExact(InputTag))
		{
			continue;
		}

		Spec.InputPressed = true;

		if (!Spec.IsActive())
		{
			TryActivateAbility(Spec.Handle);
		}

		// 하나의 입력 기능에는 하나의 실행 어빌리티 부여
		return;
	}
}

void UTBAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	ABILITYLIST_SCOPE_LOCK();

	for (FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (!Spec.Ability || !Spec.Ability->GetAssetTags().HasTagExact(InputTag))
		{
			continue;
		}

		Spec.InputPressed = false;

		if (!Spec.IsActive())
		{
			continue;
		}

		UGameplayAbility* Instance = Spec.GetPrimaryInstance();
		if (!Instance)
		{
			continue;
		}

		const FPredictionKey PredictionKey = Instance->GetCurrentActivationInfo().GetActivationPredictionKey();

		AbilitySpecInputReleased(Spec);

		// WaitInputRelease에 알림
		// 해당 태스크가 소유 클라이언트의 해제를 서버에도 전달
		InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, Spec.Handle, PredictionKey);
	}
}
