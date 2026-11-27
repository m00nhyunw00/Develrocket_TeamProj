#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "TBAbilitySystemComponent.generated.h"

class ATBPlayer;
class UGameplayAbility;

DECLARE_MULTICAST_DELEGATE(FOnPrimaryShotFired);

UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()
	
public:
	FOnPrimaryShotFired OnPrimaryShotFired;
	
	// 기본 Ability 관리 ----------------------

	void AddCharacterAbilities(const TArray<TSubclassOf<UGameplayAbility>>& StartupAbilities);

	void RemoveCharacterAbilities();

	// Ability 활성화 ----------------------

	bool TryActivateAbilityByTag(const FGameplayTag& AbilityTag);
	
	// 기능 태그에 해당하는 어빌리티의 입력 시작·해제 전달
	void AbilityInputTagPressed(const FGameplayTag& InputTag);
	void AbilityInputTagReleased(const FGameplayTag& InputTag);
	
	// 애니메이션 캔슬 ----------------------
	
	// 취소 구간에 해당하면 서버에 전환 요청 전달
	// 반환값은 요청 처리 여부이며 최종 발동 성공 여부와 구분
	bool RequestActionTransition(const FGameplayTag& InputTag, bool bMovementInput);

private:
	// 서버에서 직접 부여한 기본 Ability의 핸들 기록
	UPROPERTY(Transient)
	TArray<FGameplayAbilitySpecHandle> StartupAbilityHandles;
	
	// 요청 당시의 캐릭터와 어빌리티 실행을 확인한 뒤 전환 처리
	UFUNCTION(Server, Reliable)
	void ServerRequestActionTransition(
		ATBPlayer* RequestAvatar,
		FGameplayAbilitySpecHandle SourceHandle,
		FPredictionKey SourcePredictionKey,
		FGameplayTag InputTag,
		bool bMovementInput);
};