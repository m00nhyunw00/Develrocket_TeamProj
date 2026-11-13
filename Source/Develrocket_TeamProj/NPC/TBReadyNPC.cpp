#include "NPC/TBReadyNPC.h"

#include "Framework/TBLobbyManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"

ATBReadyNPC::ATBReadyNPC()
{
	InteractPrompt = FText::FromString(TEXT("준비 / 준비 취소"));
}

void ATBReadyNPC::Interact_Implementation(AActor* InteractInstigator)
{
	// [서버] 상호작용한 Pawn → 그 플레이어의 PlayerState
	const APawn* InstigatorPawn = Cast<APawn>(InteractInstigator);
	APlayerState* PlayerState = InstigatorPawn ? InstigatorPawn->GetPlayerState() : nullptr;

	ATBLobbyManager* Manager = FindLobbyManager();
	if (!Manager || !PlayerState)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ReadyNPC] Manager 또는 PlayerState 없음 (Instigator: %s)"),
			*GetNameSafe(InteractInstigator));
		return;
	}

	// 판정과 상태 변경은 매니저가 담당
	Manager->ToggleReady(PlayerState);
}

ATBLobbyManager* ATBReadyNPC::FindLobbyManager()
{
	if (!CachedLobbyManager.IsValid())
	{
		CachedLobbyManager = Cast<ATBLobbyManager>(
			UGameplayStatics::GetActorOfClass(this, ATBLobbyManager::StaticClass()));
	}
	return CachedLobbyManager.Get();
}
