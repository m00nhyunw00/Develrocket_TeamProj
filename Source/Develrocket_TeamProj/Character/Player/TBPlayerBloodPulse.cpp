#include "Character/Player/TBPlayer.h"

#include "Components/LineBatchComponent.h"
#include "Components/TBOutlineComponent.h"
#include "Components/TBPoolStateComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

namespace
{
	double GetBloodPulseServerTime(const UWorld& World)
	{
		const AGameStateBase* GameState = World.GetGameState();
		return GameState ? GameState->GetServerWorldTimeSeconds() : World.GetTimeSeconds();
	}
}

void ATBPlayer::ClientStartBloodPulse_Implementation(FVector_NetQuantize Origin, float MaxRadius, float ExpansionSeconds,
	double ServerStart)
{
	if (!IsLocallyControlled() || IsDead() || !GetWorld()) return;

	const double Elapsed = FMath::Max(0.0, GetBloodPulseServerTime(*GetWorld()) - ServerStart);
	if (Elapsed >= ExpansionSeconds) return;

	if (IsValid(BloodPulseEffect))
	{
		UNiagaraComponent* Effect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, BloodPulseEffect, Origin,
			FRotator::ZeroRotator, FVector::OneVector, true, false);
		if (Effect)
		{
			Effect->SetVariableFloat(TEXT("User.MaxRadius"), MaxRadius);
			Effect->SetVariableFloat(TEXT("User.ExpansionSeconds"), ExpansionSeconds);
			Effect->SetVariableFloat(TEXT("User.ElapsedOffset"), Elapsed);
			Effect->Activate(true);
			return;
		}
	}

	if (!IsValid(BloodPulseRing))
	{
		BloodPulseRing = NewObject<ULineBatchComponent>(this);
		BloodPulseRing->RegisterComponent();
	}
	BloodPulseOrigin = Origin;
	BloodPulseMaxRadius = MaxRadius;
	BloodPulseExpansionSeconds = ExpansionSeconds;
	BloodPulseServerStart = ServerStart;
}

void ATBPlayer::ClientRevealBloodPulseHits_Implementation(const TArray<FTBBloodPulseHit>& Hits)
{
	if (!IsLocallyControlled() || IsDead() || !GetWorld()) return;

	const double Now = GetBloodPulseServerTime(*GetWorld());
	for (const FTBBloodPulseHit& Hit : Hits)
	{
		AActor* Target = Hit.Target.Get();
		if (!IsValid(Target) || Target == this || Target->IsHidden() || Hit.ServerExpiration <= Now) continue;

		const UTBPoolStateComponent* Pool = Target->FindComponentByClass<UTBPoolStateComponent>();
		if (Pool ? !Pool->IsPoolActive() || Hit.PoolGeneration != Pool->GetPoolGeneration() : Hit.PoolGeneration >= 0)
		{
			continue;
		}
		if (UTBOutlineComponent* Outline = Target->FindComponentByClass<UTBOutlineComponent>())
		{
			Outline->RevealWithBloodPulse(Hit.ServerExpiration - Now);
		}
	}
}

void ATBPlayer::UpdateBloodPulseVisual()
{
	if (!IsValid(BloodPulseRing)) return;

	const double Elapsed = GetBloodPulseServerTime(*GetWorld()) - BloodPulseServerStart;
	const double Alpha = FMath::Clamp(Elapsed / BloodPulseExpansionSeconds, 0.0, 1.0);
	if (Alpha >= 1.0 || IsDead())
	{
		BloodPulseRing->DestroyComponent();
		BloodPulseRing = nullptr;
		return;
	}

	const double Radius = BloodPulseMaxRadius * Alpha;
	const FLinearColor Color(1.0f, 0.11f, 0.2f, FMath::Lerp(1.0f, 0.25f, static_cast<float>(Alpha)));
	constexpr int32 Segments = 72;
	TArray<FBatchedLine, TInlineAllocator<Segments>> Lines;
	for (int32 Index = 0; Index < Segments; ++Index)
	{
		const double StartAngle = UE_DOUBLE_TWO_PI * Index / Segments;
		const double EndAngle = UE_DOUBLE_TWO_PI * (Index + 1) / Segments;
		const FVector Start = BloodPulseOrigin + FVector(FMath::Cos(StartAngle), FMath::Sin(StartAngle), 0.0) * Radius;
		const FVector End = BloodPulseOrigin + FVector(FMath::Cos(EndAngle), FMath::Sin(EndAngle), 0.0) * Radius;
		Lines.Emplace(Start, End, Color, -1.0f, 2.0f, SDPG_World);
	}
	BloodPulseRing->Flush();
	BloodPulseRing->DrawLines(Lines);
}
