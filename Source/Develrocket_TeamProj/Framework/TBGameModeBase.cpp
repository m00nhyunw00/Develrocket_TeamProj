#include "Framework/TBGameModeBase.h"

#include "Character/Player/TBPlayer.h"
#include "Data/TBCharacterClassData.h"
#include "Framework/TBPlayerState.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

UClass* ATBGameModeBase::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	const ATBPlayerState* PS = InController ? InController->GetPlayerState<ATBPlayerState>() : nullptr;
	const UTBCharacterClassData* ClassData = PS ? PS->GetClassData() : nullptr;

	if (ClassData && ClassData->PawnClass)
	{
		return ClassData->PawnClass.Get();
	}

	// 직업 데이터가 없으면 GameMode에 지정된 기본 폰
	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

void ATBGameModeBase::RespawnPlayerInPlace(APlayerController* PlayerController)
{
	if (!IsValid(PlayerController))
	{
		return;
	}

	APawn* OldPawn = PlayerController->GetPawn();
	if (!OldPawn)
	{
		RestartPlayer(PlayerController);
		return;
	}

	// 같은 자리, 보던 방향 그대로
	FTransform SpawnTransform = OldPawn->GetActorTransform();
	SpawnTransform.SetRotation(FRotator(0.0f, PlayerController->GetControlRotation().Yaw, 0.0f).Quaternion());

	// 기존 폰을 먼저 없애야 같은 위치에 캡슐이 겹치지 않음
	PlayerController->UnPossess();
	OldPawn->Destroy();

	// GetDefaultPawnClassForController가 새 PawnClass를 반환 → Possess 시 GAS 재초기화
	RestartPlayerAtTransform(PlayerController, SpawnTransform);

	if (!PlayerController->GetPawn())
	{
		UE_LOG(LogTemp, Warning, TEXT("[TBGameMode] 제자리 스폰 실패, PlayerStart로 재시도"));
		RestartPlayer(PlayerController);
	}
}
