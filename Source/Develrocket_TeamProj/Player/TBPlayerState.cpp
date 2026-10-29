#include "TBPlayerState.h"

#include "AbilitySystemComponent.h"

ATBPlayerState::ATBPlayerState()
{
	// 캐릭터 교체 이후에도 유지할 ASC를 PlayerState에 생성
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));

	// ASC의 네트워크 복제 활성화
	AbilitySystemComponent->SetIsReplicated(true);

	// 활성 GameplayEffect의 상세 정보는 소유 클라이언트에만 복제
	// GameplayTag와 GameplayCue는 관련 클라이언트에도 복제
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
}

UAbilitySystemComponent* ATBPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent.Get();
}