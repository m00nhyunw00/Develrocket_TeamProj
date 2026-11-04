#include "Network/TBSessionSubsystem.h"

#include "OnlineSessionSettings.h"
#include "OnlineSubsystemUtils.h"

void UTBSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
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
