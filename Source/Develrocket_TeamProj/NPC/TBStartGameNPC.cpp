#include "NPC/TBStartGameNPC.h"

#include "Framework/TBLobbyManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

ATBStartGameNPC::ATBStartGameNPC()
{
	InteractPrompt = FText::FromString(TEXT("게임 시작"));
}

void ATBStartGameNPC::BeginPlay()
{
	Super::BeginPlay();
	CachedLobbyManager = Cast<ATBLobbyManager>(UGameplayStatics::GetActorOfClass(this, ATBLobbyManager::StaticClass()));

	if (!CachedLobbyManager.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[StartGameNPC] LobbyManager not found"));
	}
}

bool ATBStartGameNPC::CanInteract_Implementation(AActor* InteractInstigator) const
{
	// 부모 조건 (bCanInteract) 먼저
	if (!Super::CanInteract_Implementation(InteractInstigator))
	{
		return false;
	}

	// 호스트만
	if (!IsHostPawn(InteractInstigator))
	{
		return false;
	}

	// 시작 조건은 매니저가 판정
	const ATBLobbyManager* Manager = CachedLobbyManager.Get();
	return Manager && Manager->CanStartGame();
}

void ATBStartGameNPC::Interact_Implementation(AActor* InteractInstigator)
{
	// [서버] 클라 판정을 믿지 않고 다시 확인
	if (!IsHostPawn(InteractInstigator))
	{
		UE_LOG(LogTemp, Warning, TEXT("[StartGameNPC] 호스트가 아닌 요청 무시: %s"), *GetNameSafe(InteractInstigator));
		return;
	}

	ATBLobbyManager* Manager = CachedLobbyManager.Get();
	if (!Manager)
	{
		UE_LOG(LogTemp, Warning, TEXT("[StartGameNPC] LobbyManager 없음"));
		return;
	}

	// 인원/준비 조건은 StartGame 안에서 다시 검사
	Manager->StartGame();
}

bool ATBStartGameNPC::IsHostPawn(const AActor* InteractInstigator)
{
	const APawn* InstigatorPawn = Cast<APawn>(InteractInstigator);
	const APlayerController* PC = InstigatorPawn ? InstigatorPawn->GetController<APlayerController>() : nullptr;

	// 리슨 서버 호스트 = 이 머신에서 조종 중(Local) + 서버 권한(Authority)
	return PC && PC->IsLocalController() && PC->HasAuthority();
}