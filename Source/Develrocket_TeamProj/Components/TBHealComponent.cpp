#include "Components/TBHealComponent.h"

#include "AbilitySystemComponent.h"
#include "Framework/TBPlayerState.h"
#include "GameFramework/Pawn.h"
#include "TBGameplayTags.h"

UTBHealComponent::UTBHealComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	// 서버에서 GE가 결정되면 Blood값은 GAS가 클라이언트로 복제
}

bool UTBHealComponent::ApplyHeal()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	
	// 서버에서만 회복 처리(클라이언트가 임의로 회복하는 것 방지)
	if (!Pawn || !Pawn->HasAuthority())
	{
		return false;
	}
	
	ATBPlayerState* PS = Pawn->GetPlayerState<ATBPlayerState>();
	if (!PS)
	{
		return false;
	}
	
	UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
	
	// 사망 상태면 회복하지 않음
	if (!ASC || ASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead))
	{
		return false;
	}
	
	// 실제 GE 적용은 PlayerState가 담당
	PS->ApplyBloodRecovery();
	
	// ApplyBloodRecovery는 Void라 GE의 성공여부를 반환해 주어야함
	return false;
}


