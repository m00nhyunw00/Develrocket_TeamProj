#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "Subsystems/WorldSubsystem.h"
#include "TBCombatTokenSubsystem.generated.h"

class ATBMonster;
class UAbilitySystemComponent;

struct FTBCombatToken
{
	TWeakObjectPtr<AActor> Target;
	uint64 Id = 0;
};

UCLASS(Config = Game)
class DEVELROCKET_TEAMPROJ_API UTBCombatTokenSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	bool CanRequestAttack(ATBMonster& Attacker, AActor& Target);
	FTBCombatToken TryAcquire(ATBMonster& Attacker, AActor& Target, UAbilitySystemComponent& ASC,
		FGameplayAbilitySpecHandle Ability);
	bool IsHeld(const FTBCombatToken& Token) const;
	bool IsAttackFull(const AActor& Target) const;
	void ReleaseIfInactive(const FTBCombatToken& Token);
	void ClearTarget(ATBMonster& Attacker, TWeakObjectPtr<AActor> Target);
	void ClearAttacker(ATBMonster& Attacker);

	bool IsWaitingSlotAvailable(const AActor& Target, const ATBMonster& Attacker, int32 Slot) const;
	void SetWaitingSlot(AActor& Target, ATBMonster& Attacker, int32 Slot);
	void ClearWaitingSlot(AActor& Target, const ATBMonster& Attacker);

private:
	struct FAttack
	{
		FTBCombatToken Token;
		TWeakObjectPtr<ATBMonster> Attacker;
		TWeakObjectPtr<UAbilitySystemComponent> ASC;
		FGameplayAbilitySpecHandle Ability;
		FDelegateHandle EndedHandle;
	};

	struct FWaitingAttacker
	{
		TWeakObjectPtr<ATBMonster> Attacker;
		double LastReadyTime = 0.0;
	};

	struct FTargetState
	{
		TArray<FAttack> Attacks;
		TArray<FWaitingAttacker> Queue;
		TMap<int32, TWeakObjectPtr<ATBMonster>> WaitingSlots;
	};

	const FAttack* FindAttack(const FTBCombatToken& Token) const;
	void Release(const FTBCombatToken& Token);
	void CancelAttack(FTBCombatToken Token);
	void RemoveEmptyTarget(TWeakObjectPtr<AActor> Target);

	UPROPERTY(Config, meta = (ClampMin = "1"))
	int32 MaxSimultaneousAttacksPerTarget = 2;

	TMap<TWeakObjectPtr<AActor>, FTargetState> Targets;
	TMap<TWeakObjectPtr<ATBMonster>, FTBCombatToken> ActiveAttackers;
	uint64 NextTokenId = 0;
};
