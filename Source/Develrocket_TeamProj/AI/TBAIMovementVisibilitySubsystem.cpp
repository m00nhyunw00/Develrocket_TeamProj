#include "AI/TBAIMovementVisibilitySubsystem.h"
#include "Character/Monster/TBMonster.h"
#include "Character/Human/TBCitizen.h"
#include "Components/TBMonsterMovementComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

void UTBAIMovementVisibilitySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UWorld* World = GetWorld(); World && World->GetNetMode() != NM_Client)
		World->GetTimerManager().SetTimer(UpdateTimer, this, &ThisClass::UpdateMovementIntervals, 0.1f, true);
}

void UTBAIMovementVisibilitySubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(UpdateTimer);
	for (const auto& Weak : ThrottledCharacters)
		if (ACharacter* Character = Weak.Get()) Character->GetCharacterMovement()->SetComponentTickIntervalAndCooldown(0.0f);
	Super::Deinitialize();
}

void UTBAIMovementVisibilitySubsystem::UpdateReport(APlayerController* Viewer, const TArray<AActor*>& VisibleActors, bool bComplete)
{
	if (!IsValid(Viewer) || !GetWorld()) return;
	FViewerReport& Report = Reports.FindOrAdd(Viewer);
	Report.Time = GetWorld()->GetTimeSeconds();
	Report.bComplete = bComplete;
	Report.VisibleActors.Reset();
	if (!bComplete || VisibleActors.Num() > 256) { Report.bComplete = false; return; }
	for (AActor* Actor : VisibleActors)
	{
		if (!IsValid(Actor) || (!Actor->IsA<ATBMonster>() && !Actor->IsA<ATBCitizen>()))
		{
			Report.bComplete = false;
			Report.VisibleActors.Reset();
			return;
		}
		Report.VisibleActors.Add(Actor);
	}
}

void UTBAIMovementVisibilitySubsystem::RestoreMovementTick(ACharacter& Character)
{
	if (ThrottledCharacters.Remove(&Character) > 0)
	{
		Character.GetCharacterMovement()->SetComponentTickIntervalAndCooldown(0.0f);
	}
}

void UTBAIMovementVisibilitySubsystem::UpdateMovementIntervals()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client) return;
	TArray<const FViewerReport*> Viewers;
	bool bAllReportsValid = true;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!IsValid(PC)) continue;
		const FViewerReport* Report = Reports.Find(PC);
		if (!Report || !Report->bComplete || World->GetTimeSeconds() - Report->Time > 0.8) { bAllReportsValid = false; break; }
		Viewers.Add(Report);
	}
	if (Viewers.IsEmpty()) bAllReportsValid = false;
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		ACharacter* Character = *It;
		if (!Character->IsA<ATBMonster>() && !Character->IsA<ATBCitizen>()) continue;
		const auto* Movement = Cast<UTBMonsterMovementComponent>(Character->GetCharacterMovement());
		if (Movement && Movement->HasActiveSkillMovement())
		{
			RestoreMovementTick(*Character);
			continue;
		}
		bool bVisible = !bAllReportsValid || Character->IsHidden();
		if (!bVisible) for (const FViewerReport* Report : Viewers) if (Report->VisibleActors.Contains(Character)) { bVisible = true; break; }
		const bool bThrottled = ThrottledCharacters.Contains(Character);
		if (!bVisible && !bThrottled)
		{
			Character->GetCharacterMovement()->SetComponentTickIntervalAndCooldown(2.0f);
			ThrottledCharacters.Add(Character);
		}
		else if (bVisible && bThrottled)
		{
			RestoreMovementTick(*Character);
		}
	}
	for (auto It = ThrottledCharacters.CreateIterator(); It; ++It) if (!It->IsValid()) It.RemoveCurrent();
	for (auto It = Reports.CreateIterator(); It; ++It) if (!It.Key().IsValid()) It.RemoveCurrent();
}
