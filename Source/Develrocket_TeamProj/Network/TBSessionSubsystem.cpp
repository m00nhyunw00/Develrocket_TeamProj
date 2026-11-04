#include "Network/TBSessionSubsystem.h"
#include "OnlineSubsystemUtils.h"

void UTBSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
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
