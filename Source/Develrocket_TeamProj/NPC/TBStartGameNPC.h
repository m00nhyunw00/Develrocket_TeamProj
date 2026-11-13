#pragma once

#include "CoreMinimal.h"
#include "NPC/TBNPCBase.h"
#include "TBStartGameNPC.generated.h"

class ATBLobbyManager;

UCLASS()
class DEVELROCKET_TEAMPROJ_API ATBStartGameNPC : public ATBNPCBase
{
	GENERATED_BODY()

public:
	ATBStartGameNPC();

	virtual bool CanInteract_Implementation(AActor* InteractInstigator) const override;
	virtual void Interact_Implementation(AActor* InteractInstigator) override;

protected:
	virtual void BeginPlay() override;

private:
	// 상호작용한 Pawn이 리슨 서버 호스트의 Pawn인지
	static bool IsHostPawn(const AActor* InteractInstigator);

	TWeakObjectPtr<ATBLobbyManager> CachedLobbyManager;
};