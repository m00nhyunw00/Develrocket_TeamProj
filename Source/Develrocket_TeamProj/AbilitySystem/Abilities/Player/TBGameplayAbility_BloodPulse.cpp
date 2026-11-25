#include "AbilitySystem/Abilities/Player/TBGameplayAbility_BloodPulse.h"

#include "AbilitySystem/Attributes/TBAttributeSetBase.h"
#include "AbilitySystemComponent.h"
#include "Character/Player/TBPlayer.h"
#include "Components/PrimitiveComponent.h"
#include "Components/TBOutlineComponent.h"
#include "Components/TBPoolStateComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "TBGameplayTags.h"
#include "TimerManager.h"

namespace
{
	const FName CostMagnitudeName(TEXT("BloodPulse.BloodCost"));
	constexpr float PulseUpdateInterval = 0.05f;
	constexpr float MovingTargetLookAhead = 120.0f;
}

UTBGameplayEffect_BloodPulseCost::UTBGameplayEffect_BloodPulseCost()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FSetByCallerFloat CostMagnitude;
	CostMagnitude.DataName = CostMagnitudeName;
	FGameplayModifierInfo& Cost = Modifiers.AddDefaulted_GetRef();
	Cost.Attribute = UTBAttributeSetBase::GetBloodAttribute();
	Cost.ModifierOp = EGameplayModOp::Additive;
	Cost.ModifierMagnitude = FGameplayEffectModifierMagnitude(CostMagnitude);
}

UTBGameplayEffect_BloodPulseCooldown::UTBGameplayEffect_BloodPulseCooldown()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(8.0f));
	auto* TargetTags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("CooldownTags"));
	GEComponents.Add(TargetTags);
	FInheritedTagContainer Tags;
	Tags.AddTag(TBGameplayTags::Cooldown_Skill1);
	TargetTags->SetAndApplyTargetTagChanges(Tags);
}

UTBGameplayAbility_BloodPulse::UTBGameplayAbility_BloodPulse()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	SetAssetTags(FGameplayTagContainer(TBGameplayTags::Ability_Combat_Skill1));
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Stunned);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Draining);
	CostGameplayEffectClass = UTBGameplayEffect_BloodPulseCost::StaticClass();
	CooldownGameplayEffectClass = UTBGameplayEffect_BloodPulseCooldown::StaticClass();
}

bool UTBGameplayAbility_BloodPulse::CheckCost(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayAttribute Blood = UTBAttributeSetBase::GetBloodAttribute();
	return ASC && ASC->HasAttributeSetForAttribute(Blood) && FMath::IsFinite(BloodCost) && BloodCost >= 0.0f
		&& ASC->GetNumericAttribute(Blood) >= BloodCost + 1.0f;
}

void UTBGameplayAbility_BloodPulse::ApplyCost(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayAbilityActivationInfo ActivationInfo) const
{
	const FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(
		Handle, ActorInfo, ActivationInfo, CostGameplayEffectClass, GetAbilityLevel(Handle, ActorInfo));
	if (Spec.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(CostMagnitudeName, -BloodCost);
		ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
	}
}

void UTBGameplayAbility_BloodPulse::ApplyCooldown(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayAbilityActivationInfo ActivationInfo) const
{
	const FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(
		Handle, ActorInfo, ActivationInfo, CooldownGameplayEffectClass, GetAbilityLevel(Handle, ActorInfo));
	if (Spec.IsValid())
	{
		Spec.Data->SetDuration(CooldownSeconds, true);
		ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
	}
}

void UTBGameplayAbility_BloodPulse::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	ATBPlayer* Player = ActorInfo ? Cast<ATBPlayer>(ActorInfo->AvatarActor.Get()) : nullptr;
	UWorld* World = IsValid(Player) ? Player->GetWorld() : nullptr;
	if (!World || !Player->HasAuthority() || Player->IsDead()
		|| !FMath::IsFinite(MaxRadius) || MaxRadius <= 0.0f
		|| !FMath::IsFinite(ExpansionSeconds) || ExpansionSeconds <= 0.0f
		|| !FMath::IsFinite(CooldownSeconds) || CooldownSeconds <= 0.0f
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Origin = Player->GetActorLocation();
	StartTime = World->GetTimeSeconds();
	PreviousElapsed = 0.0;
	PreviousRadius = 0.0f;
	const AGameStateBase* GameState = World->GetGameState();
	Player->ClientStartBloodPulse(Origin, MaxRadius, ExpansionSeconds,
		GameState ? GameState->GetServerWorldTimeSeconds() : StartTime);
	World->GetTimerManager().SetTimer(WaveTimer, this, &ThisClass::AdvanceWave, PulseUpdateInterval, true);
}

void UTBGameplayAbility_BloodPulse::AdvanceWave()
{
	ATBPlayer* Player = Cast<ATBPlayer>(GetAvatarActorFromActorInfo());
	UWorld* World = IsValid(Player) ? Player->GetWorld() : nullptr;
	if (!World || Player->IsDead())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	const double Elapsed = World->GetTimeSeconds() - StartTime;
	const float Radius = MaxRadius * FMath::Clamp(Elapsed / ExpansionSeconds, 0.0, 1.0);
	const float QueryRadius = FMath::Min(MaxRadius, Radius + MovingTargetLookAhead);
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity,
		FCollisionObjectQueryParams::AllObjects, FCollisionShape::MakeSphere(QueryRadius),
		FCollisionQueryParams(SCENE_QUERY_STAT(BloodPulse), false, Player));

	const AGameStateBase* GameState = World->GetGameState();
	const double ServerNow = GameState ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
	// Use the nearest collision of each actor, even when the actor has multiple overlapping components.
	TMap<TWeakObjectPtr<AActor>, double> TargetDistances;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		const UPrimitiveComponent* Collision = Overlap.GetComponent();
		if (!IsValid(Target) || Target->IsHidden() || !IsValid(Collision) || RevealedActors.Contains(Target)) continue;
		const UTBOutlineComponent* Outline = Target->FindComponentByClass<UTBOutlineComponent>();
		if (!Outline || !Outline->CanRevealWithBloodPulse()) continue;

		FVector ClosestPoint;
		const float Distance = Collision->GetClosestPointOnCollision(Origin, ClosestPoint);
		const double DistanceSquared = Distance > 0.0f ? FMath::Square(static_cast<double>(Distance))
			: Collision->Bounds.GetBox().ComputeSquaredDistanceToPoint(Origin);
		double& Nearest = TargetDistances.FindOrAdd(Target, TNumericLimits<double>::Max());
		Nearest = FMath::Min(Nearest, DistanceSquared);
	}

	TArray<FTBBloodPulseHit> Hits;
	for (const auto& [WeakTarget, DistanceSquared] : TargetDistances)
	{
		AActor* Target = WeakTarget.Get();
		if (!Target || DistanceSquared > FMath::Square(Radius)) continue;
		const double Distance = FMath::Sqrt(DistanceSquared);
		const double* PreviouslyMeasured = PreviousDistances.Find(WeakTarget);
		const double PreviousGap = (PreviouslyMeasured ? FMath::Sqrt(*PreviouslyMeasured) : Distance) - PreviousRadius;
		if (PreviousRadius > 0.0f && PreviousGap <= 0.0) continue;
		const UTBPoolStateComponent* Pool = Target->FindComponentByClass<UTBPoolStateComponent>();
		if (Pool && !Pool->IsPoolActive()) continue;
		RevealedActors.Add(Target);
		const double SweptGap = PreviousGap + Radius - Distance;
		const double Arrival = SweptGap > UE_SMALL_NUMBER
			? FMath::Lerp(PreviousElapsed, FMath::Min<double>(Elapsed, ExpansionSeconds), PreviousGap / SweptGap)
			: PreviousElapsed;
		const float RevealSeconds = Target->IsA<ATBPlayer>() ? PlayerRevealSeconds : OtherRevealSeconds;
		const double Expiration = ServerNow + FMath::Max(0.0, RevealSeconds - (Elapsed - Arrival));
		Hits.Add({Target, Expiration, Pool ? static_cast<int64>(Pool->GetPoolGeneration()) : -1});
	}

	PreviousRadius = Radius;
	PreviousElapsed = Elapsed;
	PreviousDistances = MoveTemp(TargetDistances);
	// Limit the size of one reliable RPC in crowded areas.
	constexpr int32 MaxHitsPerRPC = 32;
	for (int32 Offset = 0; Offset < Hits.Num(); Offset += MaxHitsPerRPC)
	{
		const TArray<FTBBloodPulseHit> Batch(Hits.GetData() + Offset, FMath::Min(MaxHitsPerRPC, Hits.Num() - Offset));
		Player->ClientRevealBloodPulseHits(Batch);
	}
	if (Elapsed >= ExpansionSeconds)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UTBGameplayAbility_BloodPulse::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(WaveTimer);
	RevealedActors.Reset();
	PreviousDistances.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
