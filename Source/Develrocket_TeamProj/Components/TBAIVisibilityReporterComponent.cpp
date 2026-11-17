#include "Components/TBAIVisibilityReporterComponent.h"
#include "AI/TBAIMovementVisibilitySubsystem.h"
#include "Character/Monster/TBMonster.h"
#include "Character/Human/TBCitizen.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

UTBAIVisibilityReporterComponent::UTBAIVisibilityReporterComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UTBAIVisibilityReporterComponent::BeginPlay()
{
	Super::BeginPlay();
	GetWorld()->GetTimerManager().SetTimer(ReportTimer, this, &ThisClass::ReportVisibleAI, 0.2f, true);
}

void UTBAIVisibilityReporterComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(ReportTimer);
	Super::EndPlay(Reason);
}

void UTBAIVisibilityReporterComponent::ReportVisibleAI()
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	APlayerController* PC = OwnerPawn ? Cast<APlayerController>(OwnerPawn->GetController()) : nullptr;
	if (!PC || !PC->IsLocalController()) return;
	int32 Width = 0, Height = 0;
	PC->GetViewportSize(Width, Height);
	if (Width <= 0 || Height <= 0) return;
	FVector Eye; FRotator Rotation;
	PC->GetPlayerViewPoint(Eye, Rotation);
	TArray<AActor*> Visible;
	bool bComplete = true;
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		ACharacter* Candidate = *It;
		if ((!Candidate->IsA<ATBMonster>() && !Candidate->IsA<ATBCitizen>()) || Candidate->IsHidden()) continue;
		const FBox Box = Candidate->GetComponentsBoundingBox(false);
		if (!Box.IsValid) { bComplete = false; break; }
		bool bOnScreen = Box.IsInsideOrOn(Eye);
		bool bFront = false, bBehind = false;
		FVector2D Min(FLT_MAX, FLT_MAX), Max(-FLT_MAX, -FLT_MAX);
		for (int32 Index = 0; Index < 8 && !bOnScreen; ++Index)
		{
			const FVector Corner((Index & 1) ? Box.Max.X : Box.Min.X, (Index & 2) ? Box.Max.Y : Box.Min.Y, (Index & 4) ? Box.Max.Z : Box.Min.Z);
			if (FVector::DotProduct(Corner - Eye, Rotation.Vector()) <= 0.0) { bBehind = true; continue; }
			bFront = true;
			FVector2D Screen;
			if (!PC->ProjectWorldLocationToScreen(Corner, Screen, false)) { bOnScreen = true; break; }
			Min.X = FMath::Min(Min.X, Screen.X); Min.Y = FMath::Min(Min.Y, Screen.Y);
			Max.X = FMath::Max(Max.X, Screen.X); Max.Y = FMath::Max(Max.Y, Screen.Y);
		}
		// Near-plane intersections are treated as visible, conservatively.
		bOnScreen |= bFront && (bBehind || (Max.X >= 0 && Max.Y >= 0 && Min.X <= Width && Min.Y <= Height));
		if (bOnScreen)
		{
			if (Visible.Num() >= 256) { bComplete = false; break; }
			Visible.Add(Candidate);
		}
	}
	ServerReportVisibleAI(Visible, bComplete);
}

void UTBAIVisibilityReporterComponent::ServerReportVisibleAI_Implementation(const TArray<AActor*>& VisibleActors, bool bComplete)
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	APlayerController* PC = OwnerPawn ? Cast<APlayerController>(OwnerPawn->GetController()) : nullptr;
	if (PC && GetWorld())
	{
		if (UTBAIMovementVisibilitySubsystem* Manager = GetWorld()->GetSubsystem<UTBAIMovementVisibilitySubsystem>())
			Manager->UpdateReport(PC, VisibleActors, bComplete && VisibleActors.Num() <= 256);
	}
}
