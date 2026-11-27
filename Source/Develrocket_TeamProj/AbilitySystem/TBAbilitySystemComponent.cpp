#include "AbilitySystem/TBAbilitySystemComponent.h"

#include "Abilities/GameplayAbility.h"
#include "Abilities/TBGameplayAbility_Action.h"
#include "Character/Player/TBPlayer.h"

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

	if (RequestActionTransition(AbilityTag, false))
	{
		return true;
	}

	const FGameplayTagContainer AbilityTags(AbilityTag);
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

		if (RequestActionTransition(InputTag, false))
		{
			return;
		}

		if (!Spec.IsActive())
		{
			TryActivateAbility(Spec.Handle);
		}

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

bool UTBAbilitySystemComponent::RequestActionTransition(const FGameplayTag& InputTag, bool bMovementInput)
{
	ATBPlayer* Player = Cast<ATBPlayer>(GetAvatarActor());
	UTBGameplayAbility_Action* Action = Cast<UTBGameplayAbility_Action>(GetAnimatingAbility());

	if (!Player || !Player->IsLocallyControlled()
		|| Player->IsActionBlocked()
		|| !Action
		|| !Action->CanCancelByInput(InputTag, bMovementInput))
	{
		return false;
	}

	ServerRequestActionTransition(
		Player,
		Action->GetCurrentAbilitySpecHandle(),
		Action->GetCurrentActivationInfo().GetActivationPredictionKey(),
		InputTag,
		bMovementInput);

	return true;
}

void UTBAbilitySystemComponent::ServerRequestActionTransition_Implementation(ATBPlayer* RequestAvatar,
	FGameplayAbilitySpecHandle SourceHandle, FPredictionKey SourcePredictionKey, FGameplayTag InputTag,
	bool bMovementInput)
{
    ABILITYLIST_SCOPE_LOCK();

    ATBPlayer* Player = Cast<ATBPlayer>(GetAvatarActor());
    UTBGameplayAbility_Action* Action =
        Cast<UTBGameplayAbility_Action>(GetAnimatingAbility());

    // 이전 캐릭터 또는 이전 실행에서 늦게 도착한 요청 제외
    if (!Player || Player != RequestAvatar || Player->IsActionBlocked()
        || !Action
        || Action->GetCurrentAbilitySpecHandle() != SourceHandle
        || !(Action->GetCurrentActivationInfo().GetActivationPredictionKey()
            == SourcePredictionKey)
        || !Action->CanCancelByInput(InputTag, bMovementInput))
    {
        return;
    }

    FGameplayAbilitySpecHandle TargetHandle;

    if (!bMovementInput)
    {
        for (FGameplayAbilitySpec& Spec : GetActivatableAbilities())
        {
            if (!Spec.Ability || Spec.PendingRemove
                || Spec.RemoveAfterActivation
                || !Spec.Ability->GetAssetTags().HasTagExact(InputTag))
            {
                continue;
            }

            // 별도로 실행 중인 어빌리티의 중복 발동 제외
            if (Spec.IsActive() && Spec.Handle != SourceHandle)
            {
                return;
            }

            UGameplayAbility* TargetAbility = Spec.GetPrimaryInstance();
            if (!TargetAbility)
            {
                TargetAbility = Spec.Ability;
            }

            // 현재 동작을 끝내기 전 쿨다운·자원 확인
            if (!TargetAbility->CheckCooldown(Spec.Handle, AbilityActorInfo.Get())
                || !TargetAbility->CheckCost(Spec.Handle, AbilityActorInfo.Get())
                || IsAbilityInputBlocked(Spec.InputID))
            {
                return;
            }

            TargetHandle = Spec.Handle;
            break;
        }

        if (!TargetHandle.IsValid())
        {
            return;
        }
    }

    // 서버에서 검증한 현재 실행 인스턴스만 취소
    Action->CancelAbility(
        SourceHandle,
        Action->GetCurrentActorInfo(),
        Action->GetCurrentActivationInfo(),
        true);

    if (Action->IsActive())
    {
        return;
    }

    if (TargetHandle.IsValid())
    {
        // GAS의 활성화 조건 재검사 및 후속 어빌리티 실행
        TryActivateAbility(TargetHandle);
    }
}

