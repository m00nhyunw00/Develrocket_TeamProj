#include "Framework/TBGameModeBase.h"

#include "CHaracter/Player/TBPlayer.h"
#include "Data/TBCharacterClassData.h"
#include "Framework/TBPlayerState.h"
#include "GameFramework/Controller.h"

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