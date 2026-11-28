#include "Character/Player/TBPlayer.h"

#include "Camera/CameraComponent.h"
#include "Components/TBOutlineComponent.h"
#include "Components/TBPoolStateComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	constexpr double PulseEndHoldSeconds = 0.05;
	const FName PulseOriginParameter(TEXT("PulseOrigin"));
	const FName PulseRadiusParameter(TEXT("PulseRadius"));
	const FName PulseOpacityParameter(TEXT("PulseOpacity"));

	double GetBloodPulseServerTime(const UWorld& World)
	{
		const AGameStateBase* GameState = World.GetGameState();
		return GameState ? GameState->GetServerWorldTimeSeconds() : World.GetTimeSeconds();
	}
}

// 이거 Q 플레이어캐릭터들의 스킬인데 여기있는거 쫌 에바임 나중에 Pulse어빌리티쪽으로 빼야겠다
// 이거 Q 플레이어캐릭터들의 스킬인데 여기있는거 쫌 에바임 나중에 Pulse어빌리티쪽으로 빼야겠다
// 이거 Q 플레이어캐릭터들의 스킬인데 여기있는거 쫌 에바임 나중에 Pulse어빌리티쪽으로 빼야겠다
// 이거 Q 플레이어캐릭터들의 스킬인데 여기있는거 쫌 에바임 나중에 Pulse어빌리티쪽으로 빼야겠다
// 이거 Q 플레이어캐릭터들의 스킬인데 여기있는거 쫌 에바임 나중에 Pulse어빌리티쪽으로 빼야겠다
// 이거 Q 플레이어캐릭터들의 스킬인데 여기있는거 쫌 에바임 나중에 Pulse어빌리티쪽으로 빼야겠다
// 이거 Q 플레이어캐릭터들의 스킬인데 여기있는거 쫌 에바임 나중에 Pulse어빌리티쪽으로 빼야겠다
// 이거 Q 플레이어캐릭터들의 스킬인데 여기있는거 쫌 에바임 나중에 Pulse어빌리티쪽으로 빼야겠다
// 이거 Q 플레이어캐릭터들의 스킬인데 여기있는거 쫌 에바임 나중에 Pulse어빌리티쪽으로 빼야겠다
// 이거 Q 플레이어캐릭터들의 스킬인데 여기있는거 쫌 에바임 나중에 Pulse어빌리티쪽으로 빼야겠다
// 이거 Q 플레이어캐릭터들의 스킬인데 여기있는거 쫌 에바임 나중에 Pulse어빌리티쪽으로 빼야겠다
// 이거 Q 플레이어캐릭터들의 스킬인데 여기있는거 쫌 에바임 나중에 Pulse어빌리티쪽으로 빼야겠다

void ATBPlayer::ClientStartBloodPulse_Implementation(FVector_NetQuantize Origin, float MaxRadius, float ExpansionSeconds,
	double ServerStart)
{
	if (!IsLocallyControlled() || IsDead() || !GetWorld() || !FirstPersonCamera || !BloodPulseMaterial) return;

	const double Elapsed = FMath::Max(0.0, GetBloodPulseServerTime(*GetWorld()) - ServerStart);
	if (Elapsed >= ExpansionSeconds) return;

	if (!BloodPulseMaterialInstance)
	{
		BloodPulseMaterialInstance = UMaterialInstanceDynamic::Create(BloodPulseMaterial, this);
	}
	if (!BloodPulseMaterialInstance) return;

	BloodPulseMaxRadius = MaxRadius;
	BloodPulseExpansionSeconds = ExpansionSeconds;
	BloodPulseServerStart = ServerStart;
	bBloodPulseReachedMaxRadius = false;
	BloodPulseMaterialInstance->SetVectorParameterValue(PulseOriginParameter, FLinearColor(Origin.X, Origin.Y, Origin.Z));
	UpdateBloodPulseVisual();
	FirstPersonCamera->AddOrUpdateBlendable(BloodPulseMaterialInstance.Get(), 1.0f);
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
	if (BloodPulseExpansionSeconds <= 0.0f) return;

	const double Elapsed = GetBloodPulseServerTime(*GetWorld()) - BloodPulseServerStart;
	if (IsDead() || (bBloodPulseReachedMaxRadius && Elapsed >= BloodPulseExpansionSeconds + PulseEndHoldSeconds))
	{
		FirstPersonCamera->RemoveBlendable(BloodPulseMaterialInstance.Get());
		BloodPulseExpansionSeconds = 0.0f;
		return;
	}

	const double Alpha = FMath::Clamp(Elapsed / BloodPulseExpansionSeconds, 0.0, 1.0);
	bBloodPulseReachedMaxRadius |= Alpha >= 1.0;
	BloodPulseMaterialInstance->SetScalarParameterValue(PulseRadiusParameter, BloodPulseMaxRadius * Alpha);
	BloodPulseMaterialInstance->SetScalarParameterValue(PulseOpacityParameter, FMath::Lerp(1.0, 0.25, Alpha));
}
