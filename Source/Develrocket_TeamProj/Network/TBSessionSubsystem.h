#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TBSessionSubsystem.generated.h"

UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	UFUNCTION(BlueprintCallable)
	void HostSession(int32 NumPublicConnections);
	
private: 
	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	
private:
	IOnlineSessionPtr SessionInterface;
	
	FOnCreateSessionCompleteDelegate CreateSessionCompleteDelegate;
	FDelegateHandle CreateSessionCompleteDelegateHandle;
	
	FString LobbyMapPath = TEXT("/Game/Developers/Dev_Choi/MainMenu/Maps/L_Lobby"); //Dev_Choi바깥으로 옮기면 해당 위치를 수정해 주어야함
	
};