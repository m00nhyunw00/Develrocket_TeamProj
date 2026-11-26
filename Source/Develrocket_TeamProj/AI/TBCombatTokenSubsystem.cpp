#include "AI/TBCombatTokenSubsystem.h"

#include "AbilitySystemComponent.h"
#include "Character/Monster/TBMonster.h"
#include "Engine/World.h"

namespace
{
	// The combat service refreshes ready attackers every 0.2 seconds during pursuit.
	constexpr double ReadyRequestLifetime = 0.8;
}

bool UTBCombatTokenSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return Super::ShouldCreateSubsystem(Outer) && World && World->IsGameWorld() && World->GetNetMode() != NM_Client;
}

void UTBCombatTokenSubsystem::Deinitialize()
{
	for (const auto& Pair : Targets)
	{
		for (const FAttack& Attack : Pair.Value.Attacks)
		{
			if (UAbilitySystemComponent* ASC = Attack.ASC.Get()) ASC->OnAbilityEnded.Remove(Attack.EndedHandle);
		}
	}
	Targets.Reset();
	ActiveAttackers.Reset();
	Super::Deinitialize();
}

bool UTBCombatTokenSubsystem::CanRequestAttack(ATBMonster& Attacker, AActor& Target)
{
	if (!Attacker.HasAuthority() || Attacker.IsDead() || !IsValid(&Target) || &Attacker == &Target || ActiveAttackers.Contains(&Attacker))
	{
		return false;
	}

	FTargetState& State = Targets.FindOrAdd(&Target);
	const double Now = GetWorld()->GetTimeSeconds();
	State.Queue.RemoveAll([Now](const FWaitingAttacker& Entry)
	{
		const ATBMonster* Monster = Entry.Attacker.Get();
		return !Monster || Monster->IsDead() || Now - Entry.LastReadyTime > ReadyRequestLifetime;
	});
	const int32 ExistingIndex = State.Queue.IndexOfByPredicate([&Attacker](const FWaitingAttacker& Entry)
	{
		return Entry.Attacker == &Attacker;
	});
	const int32 QueueIndex = ExistingIndex == INDEX_NONE ? State.Queue.Add({&Attacker, Now}) : ExistingIndex;
	State.Queue[QueueIndex].LastReadyTime = Now;
	return QueueIndex < FMath::Max(1, MaxSimultaneousAttacksPerTarget) - State.Attacks.Num();
}

FTBCombatToken UTBCombatTokenSubsystem::TryAcquire(ATBMonster& Attacker, AActor& Target, UAbilitySystemComponent& ASC,
	FGameplayAbilitySpecHandle Ability)
{
	if (!Ability.IsValid() || !CanRequestAttack(Attacker, Target)) return {};

	ClearWaitingSlot(Target, Attacker);
	FTargetState& State = Targets.FindChecked(&Target);
	State.Queue.RemoveAll([&Attacker](const FWaitingAttacker& Entry) { return Entry.Attacker == &Attacker; });
	const FTBCombatToken Token{&Target, ++NextTokenId};
	FAttack& Attack = State.Attacks.AddDefaulted_GetRef();
	Attack.Token = Token;
	Attack.Attacker = &Attacker;
	Attack.ASC = &ASC;
	Attack.Ability = Ability;
	Attack.EndedHandle = ASC.OnAbilityEnded.AddWeakLambda(this, [this, Token, Ability](const FAbilityEndedData& Ended)
	{
		if (Ended.AbilitySpecHandle == Ability) Release(Token);
	});
	ActiveAttackers.Add(&Attacker, Token);
	return Token;
}

const UTBCombatTokenSubsystem::FAttack* UTBCombatTokenSubsystem::FindAttack(const FTBCombatToken& Token) const
{
	const FTargetState* State = Token.Id ? Targets.Find(Token.Target) : nullptr;
	return State ? State->Attacks.FindByPredicate([&Token](const FAttack& Attack) { return Attack.Token.Id == Token.Id; }) : nullptr;
}

bool UTBCombatTokenSubsystem::IsHeld(const FTBCombatToken& Token) const
{
	return FindAttack(Token) != nullptr;
}

bool UTBCombatTokenSubsystem::IsAttackFull(const AActor& Target) const
{
	const FTargetState* State = Targets.Find(&Target);
	return State && State->Attacks.Num() >= FMath::Max(1, MaxSimultaneousAttacksPerTarget);
}

void UTBCombatTokenSubsystem::ReleaseIfInactive(const FTBCombatToken& Token)
{
	if (const FAttack* Attack = FindAttack(Token))
	{
		UAbilitySystemComponent* ASC = Attack->ASC.Get();
		const FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecFromHandle(Attack->Ability) : nullptr;
		if (!Spec || !Spec->IsActive()) Release(Token);
	}
}

void UTBCombatTokenSubsystem::CancelAttack(FTBCombatToken Token)
{
	if (const FAttack* Attack = FindAttack(Token))
	{
		if (UAbilitySystemComponent* ASC = Attack->ASC.Get()) ASC->CancelAbilityHandle(Attack->Ability);
		ReleaseIfInactive(Token);
	}
}

void UTBCombatTokenSubsystem::Release(const FTBCombatToken& Token)
{
	if (const FAttack* Attack = FindAttack(Token))
	{
		if (UAbilitySystemComponent* ASC = Attack->ASC.Get()) ASC->OnAbilityEnded.Remove(Attack->EndedHandle);
		ActiveAttackers.Remove(Attack->Attacker);
		Targets.FindChecked(Token.Target).Attacks.RemoveAll([&Token](const FAttack& Entry) { return Entry.Token.Id == Token.Id; });
		RemoveEmptyTarget(Token.Target);
	}
}

void UTBCombatTokenSubsystem::ClearTarget(ATBMonster& Attacker, TWeakObjectPtr<AActor> TargetKey)
{
	if (FTargetState* State = Targets.Find(TargetKey))
	{
		State->Queue.RemoveAll([&Attacker](const FWaitingAttacker& Entry) { return Entry.Attacker == &Attacker; });
		for (auto It = State->WaitingSlots.CreateIterator(); It; ++It)
		{
			if (It.Value() == &Attacker) It.RemoveCurrent();
		}
		RemoveEmptyTarget(TargetKey);
	}
	if (const FTBCombatToken* Token = ActiveAttackers.Find(&Attacker); Token && Token->Target.HasSameIndexAndSerialNumber(TargetKey))
	{
		CancelAttack(*Token);
	}
}

void UTBCombatTokenSubsystem::ClearAttacker(ATBMonster& Attacker)
{
	for (auto It = Targets.CreateIterator(); It; ++It)
	{
		FTargetState& State = It.Value();
		State.Queue.RemoveAll([&Attacker](const FWaitingAttacker& Entry) { return !Entry.Attacker.IsValid() || Entry.Attacker == &Attacker; });
		for (auto Slot = State.WaitingSlots.CreateIterator(); Slot; ++Slot)
		{
			if (!Slot.Value().IsValid() || Slot.Value() == &Attacker) Slot.RemoveCurrent();
		}
		if (State.Attacks.IsEmpty() && State.Queue.IsEmpty() && State.WaitingSlots.IsEmpty()) It.RemoveCurrent();
	}
	if (const FTBCombatToken* Token = ActiveAttackers.Find(&Attacker)) CancelAttack(*Token);
}

bool UTBCombatTokenSubsystem::IsWaitingSlotAvailable(const AActor& Target, const ATBMonster& Attacker, int32 Slot) const
{
	const FTargetState* State = Targets.Find(&Target);
	const TWeakObjectPtr<ATBMonster>* Occupant = State ? State->WaitingSlots.Find(Slot) : nullptr;
	return !Occupant || !Occupant->IsValid() || Occupant->Get() == &Attacker;
}

void UTBCombatTokenSubsystem::SetWaitingSlot(AActor& Target, ATBMonster& Attacker, int32 Slot)
{
	ClearWaitingSlot(Target, Attacker);
	Targets.FindOrAdd(&Target).WaitingSlots.Add(Slot, &Attacker);
}

void UTBCombatTokenSubsystem::ClearWaitingSlot(AActor& Target, const ATBMonster& Attacker)
{
	if (FTargetState* State = Targets.Find(&Target))
	{
		for (auto It = State->WaitingSlots.CreateIterator(); It; ++It)
		{
			if (It.Value().Get() == &Attacker) It.RemoveCurrent();
		}
		RemoveEmptyTarget(&Target);
	}
}

void UTBCombatTokenSubsystem::RemoveEmptyTarget(TWeakObjectPtr<AActor> Target)
{
	const FTargetState* State = Targets.Find(Target);
	if (State && State->Attacks.IsEmpty() && State->Queue.IsEmpty() && State->WaitingSlots.IsEmpty()) Targets.Remove(Target);
}
