#pragma once

#include "CoreMinimal.h"
#include "NPC/TBNPCBase.h"
#include "TBReadyNPC.generated.h"

class ATBLobbyManager;

UCLASS()
class DEVELROCKET_TEAMPROJ_API ATBReadyNPC : public ATBNPCBase
{
	GENERATED_BODY()
	
public:
	ATBReadyNPC();
	
	virtual void Interact_Implementation(AActor* InteractInstigator) override;
	
private:
	// 맵의 LobbyManager를 한 번 찾아서 캐시
	ATBLobbyManager* FindLobbyManager();
	
	TWeakObjectPtr<ATBLobbyManager> CachedLobbyManager;
	
};
