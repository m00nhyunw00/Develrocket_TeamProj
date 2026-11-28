#include "Framework/TBGameModeBase.h"

#include "Character/Player/TBPlayer.h"
#include "Data/TBCharacterClassData.h"
#include "Engine/GameInstance.h"
#include "Framework/TBClassSelectionSubsystem.h"
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

FString ATBGameModeBase::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId,
	const FString& Options, const FString& Portal)
{
	const FString Result = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);

	// PlayerState는 생성됨, 폰은 아직 없음
	// 여기서 직업을 넣으면 이후 RestartPlayer가 해당 직업 폰으로 스폰
	const UTBClassSelectionSubsystem* Selection = GetGameInstance()->GetSubsystem<UTBClassSelectionSubsystem>();
	ATBPlayerState* PS = NewPlayerController ? NewPlayerController->GetPlayerState<ATBPlayerState>() : nullptr;
	if (!Selection || !PS)
	{
		return Result;
	}

	// 보관된 직업이 없으면 기본 직업 유지 (로비를 거치지 않은 단독 실행 등)
	if (const UTBCharacterClassData* ClassData = Selection->FindSelection(UniqueId))
	{
		PS->SetSelectedClassData(ClassData);
		UE_LOG(LogTemp, Log, TEXT("[ClassSelection] 복원: %s → %s"), *PS->GetPlayerName(), *ClassData->GetName());
	}

	return Result;
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
