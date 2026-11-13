#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TBSessionSubsystem.generated.h"

USTRUCT (BlueprintType)
struct FTBSessionInfo
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly)
	int32 SearchIndex = INDEX_NONE;
	
	UPROPERTY(BlueprintReadOnly)
	FString HostName;
	
	UPROPERTY(BlueprintReadOnly)
	int32 CurrentPlayers = 0;
	
	UPROPERTY(BlueprintReadOnly)
	int32 MaxPlayers = 0;
	
	UPROPERTY(BLueprintReadOnly)
	int32 PingInMs = 0;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnTBSessionsFound, const TArray<FTBSessionInfo>& /*Sessions*/, bool /*bWasSuccessful*/);

UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	UFUNCTION(BlueprintCallable)
	void HostSession(int32 NumPublicConnections);
	UFUNCTION(BlueprintCallable)
	void FindSessions(int32 MaxSearchResults);
	UFUNCTION(BlueprintCallable)
	void JoinSessionByIndex(int32 SearchIndex);
	
	FOnTBSessionsFound OnSessionsFound;
	
	
private: 
	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnFindSessionsComplete(bool bWasSuccessful);
	void JoinFoundSession(const FOnlineSessionSearchResult& SearchResult);
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	
private:
	IOnlineSessionPtr SessionInterface;
	
	FOnCreateSessionCompleteDelegate CreateSessionCompleteDelegate;
	FDelegateHandle CreateSessionCompleteDelegateHandle;
	
	FOnFindSessionsCompleteDelegate FindSessionsCompleteDelegate;
	FDelegateHandle FindSessionsCompleteDelegateHandle;
	
	FOnJoinSessionCompleteDelegate JoinSessionCompleteDelegate;
	FDelegateHandle JoinSessionCompleteDelegateHandle;
	
	TSharedPtr<FOnlineSessionSearch> SessionSearch;
	
	FString LobbyMapPath = TEXT("/Game/_Game/Maps/L_MatchWait"); 
	
};