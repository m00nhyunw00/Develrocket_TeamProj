#include "Framework/TBClassSelectionSubsystem.h"

#include "Data/TBCharacterClassData.h"
#include "GameFramework/OnlineReplStructs.h"
#include "GameFramework/PlayerState.h"

void UTBClassSelectionSubsystem::SaveSelection(const APlayerState* PlayerState, const UTBCharacterClassData* ClassData)
{
	if (!PlayerState || !ClassData)
	{
		return;
	}

	const FUniqueNetIdRepl& UniqueId = PlayerState->GetUniqueId();
	if (!UniqueId.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ClassSelection] 고유 ID 없음, 저장 생략: %s"), *PlayerState->GetPlayerName());
		return;
	}

	Selections.Add(UniqueId.ToString(), ClassData);
	UE_LOG(LogTemp, Log, TEXT("[ClassSelection] 저장: %s → %s"), *PlayerState->GetPlayerName(), *ClassData->GetName());
}

const UTBCharacterClassData* UTBClassSelectionSubsystem::FindSelection(const FUniqueNetIdRepl& UniqueId) const
{
	if (!UniqueId.IsValid())
	{
		return nullptr;
	}

	const TObjectPtr<const UTBCharacterClassData>* Found = Selections.Find(UniqueId.ToString());
	return Found ? Found->Get() : nullptr;
}
