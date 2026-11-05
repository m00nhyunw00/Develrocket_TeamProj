#include "Network/TBSessionSubsystem.h"

#include "OnlineSessionSettings.h"
#include "OnlineSubsystemUtils.h"

void UTBSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	FindSessionsCompleteDelegate.BindUObject(this, &UTBSessionSubsystem::OnFindSessionsComplete);
	JoinSessionCompleteDelegate.BindUObject(this, &UTBSessionSubsystem::OnJoinSessionComplete);
	CreateSessionCompleteDelegate.BindUObject(this, &UTBSessionSubsystem::OnCreateSessionComplete);
	
	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(GetWorld());
	if (!OnlineSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TBSession] OnlineSubsystem not found"));
		return;
	}
	
	SessionInterface = OnlineSubsystem->GetSessionInterface();
	
	UE_LOG(LogTemp, Log, TEXT("[TBSession] Subsystem: %s / Valid: %s"),
		*OnlineSubsystem->GetSubsystemName().ToString(),
		SessionInterface.IsValid() ? TEXT("true") : TEXT("false"));
}

void UTBSessionSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UTBSessionSubsystem::HostSession(int32 NumPublicConnections)
{
	UE_LOG(LogTemp, Log, TEXT("[TBSession] HostSession called (Num=%d)"), NumPublicConnections); 
	
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[TBSession] HostSession Failed: SessionInterface invalid"));
		return;
	}
	
	FOnlineSessionSettings SessionSettings;
	SessionSettings.NumPublicConnections = NumPublicConnections;
	SessionSettings.bIsLANMatch = true;
	SessionSettings.bShouldAdvertise = true;
	SessionSettings.bAllowJoinInProgress = true; // 데스 매치이지만 우선은 참가가능하도록
	SessionSettings.bUsesPresence = true;
	SessionSettings.bUseLobbiesIfAvailable = true;
	SessionSettings.bIsDedicated = false;
	
	CreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegate);
	
	const bool bRequested = SessionInterface->CreateSession(0, NAME_GameSession, SessionSettings);
	
	if (!bRequested)
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		UE_LOG(LogTemp, Warning, TEXT("[TBSession] CreateSession request failed"));
	}
}

void UTBSessionSubsystem::FindSessions(int32 MaxSearchResults)
{
	UE_LOG(LogTemp, Log, TEXT("[TBSession] FindSessions called (Num=%d)"), MaxSearchResults);
	
	if (!SessionInterface.IsValid())
	{
		OnSessionsFound.Broadcast(TArray<FTBSessionInfo>(), false);
		UE_LOG(LogTemp, Warning, TEXT("[TBSession] FindSession Failed: SessionInterface invalid"));
		return;
	}
	
	SessionSearch = MakeShared<FOnlineSessionSearch>();
	SessionSearch->MaxSearchResults = MaxSearchResults;
	SessionSearch->bIsLanQuery = true;
	
	FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);
	
	const bool bRequested = SessionInterface->FindSessions(0, SessionSearch.ToSharedRef());
	
	if (!bRequested)
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		OnSessionsFound.Broadcast(TArray<FTBSessionInfo>(), false);
		UE_LOG(LogTemp, Warning, TEXT("[TBSession] FindSessions request failed"));
	}
}

void UTBSessionSubsystem::JoinSessionByIndex(int32 SearchIndex)
{
	//검색결과가 없거나, 번호가 배열 범위를 벗어나면 중단 (크래시 방지)
	if (!SessionSearch.IsValid() || !SessionSearch->SearchResults.IsValidIndex(SearchIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[TBSession] JoinSessionByIndex: invalid index %d"), SearchIndex);
		return;
	}
	
	JoinFoundSession(SessionSearch->SearchResults[SearchIndex]);
}

void UTBSessionSubsystem::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
	}
	
	UE_LOG(LogTemp, Log, TEXT("[TBSession] OnCreateSessionComplete: %s / Success: %s"),
		*SessionName.ToString(), bWasSuccessful ? TEXT("true") : TEXT("false"));
	
	if (!bWasSuccessful)
	{
		return;
	}
	
	if (UWorld* World = GetWorld())
	{
		World->ServerTravel(LobbyMapPath + TEXT("?listen"));
	}
}

void UTBSessionSubsystem::OnFindSessionsComplete(bool bWasSuccessful)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
	}
	
	TArray<FTBSessionInfo> Sessions;
	
	if (!SessionSearch.IsValid() || !bWasSuccessful)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TBSession] FindSessions failed"));
		OnSessionsFound.Broadcast(Sessions, false);
		return;
	}
	
	for (int32 i = 0; i < SessionSearch->SearchResults.Num();++i)
	{
		const FOnlineSessionSearchResult& Result = SessionSearch->SearchResults[i];
		
		FTBSessionInfo Info;
		Info.SearchIndex = i;
		Info.HostName = Result.Session.OwningUserName;                   // 방 만든 사람 이름
		Info.MaxPlayers = Result.Session.SessionSettings.NumPublicConnections; // Host 때 넣은 최대 인원
		Info.CurrentPlayers = Info.MaxPlayers - Result.Session.NumOpenPublicConnections; // 최대 - 빈자리 = 현재 인원
		Info.PingInMs = Result.PingInMs;   // 핑
		
		Sessions.Add(Info);
	}
	
	UE_LOG(LogTemp,Log, TEXT("[TBSession] Found %d session(s)"), Sessions.Num());
	
	OnSessionsFound.Broadcast(Sessions, true);
}

void UTBSessionSubsystem::JoinFoundSession(const FOnlineSessionSearchResult& SearchResult)
{
	UE_LOG(LogTemp, Log, TEXT("[TBSession] JoinFoundSession Called"));
	
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[TBSession] JoinFoundSession Failed: SessionInterface invalid"));
		return;
	}
	
	JoinSessionCompleteDelegateHandle=SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);
	
	const bool bRequested = SessionInterface->JoinSession(0, NAME_GameSession, SearchResult);
	
	if (!bRequested)
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		UE_LOG(LogTemp, Warning, TEXT("[TBSession] JoinSession request failed"));
	}
}

void UTBSessionSubsystem::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
	}
	
	UE_LOG(LogTemp, Log, TEXT("[TBSession] OnJoinSessionComplete: %s / Result: %s"),
		*SessionName.GetPlainNameString(), LexToString(Result));
	
	if (Result != EOnJoinSessionCompleteResult::Success)
	{
		return;
	}
	
	FString ConnectString;
	if (!SessionInterface.IsValid() || !SessionInterface->GetResolvedConnectString(SessionName, ConnectString))
	{
		UE_LOG(LogTemp, Warning, TEXT("[TBSession] Could not resolve connect string"));
		return;
	}
	
	UE_LOG(LogTemp, Log, TEXT("[TBSession] ConnectString: %s"), *ConnectString);
	
	UGameInstance* GI = GetGameInstance();
	APlayerController* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr;
	
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TBSession] No local PlayerController"));
		return;
	}
	
	PC->ClientTravel(ConnectString, TRAVEL_Absolute);
}	
