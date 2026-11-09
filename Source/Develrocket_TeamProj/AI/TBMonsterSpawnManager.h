#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TBMonsterSpawnManager.generated.h"

class ATBMonster;
class ACityLayoutCoordinator;

UCLASS()
class DEVELROCKET_TEAMPROJ_API ATBMonsterSpawnManager : public AActor
{
	GENERATED_BODY()

public:
	ATBMonsterSpawnManager();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditInstanceOnly, Category="Monster Spawn")
	TSubclassOf<ATBMonster> MonsterClass;

	UPROPERTY(EditInstanceOnly, Category="Monster Spawn", meta=(ClampMin="0"))
	int32 DesiredMonsterCount = 5;

	UPROPERTY(EditInstanceOnly, Category="Monster Spawn", meta=(ClampMin="0.0", Units="s"))
	float RespawnDelay = 10.0f;

	UPROPERTY(EditInstanceOnly, Category="Monster Spawn", meta=(ClampMin="0.1", Units="s"))
	float RetryInterval = 1.0f;

	UPROPERTY(EditInstanceOnly, Category="Monster Spawn", meta=(ClampMin="1"))
	int32 MaxCandidatesPerAttempt = 10;

	UPROPERTY(EditInstanceOnly, Category="Monster Spawn")
	FName PatrolPointTag = TEXT("CityPatrolPoint");

	UPROPERTY(EditInstanceOnly, Category="Monster Spawn", meta=(ClampMin="0.0", Units="cm"))
	float MinimumPlayerDistance = 1500.0f;

	UPROPERTY(EditInstanceOnly, Category="Monster Spawn", meta=(ClampMin="0.0", Units="cm"))
	float MinimumMonsterDistance = 300.0f;

	UPROPERTY(EditInstanceOnly, Category="Monster Spawn", meta=(ClampMin="0.0", Units="cm"))
	float NavProjectionExtent = 250.0f;

private:
	UFUNCTION()
	void HandleCityReady(int32 Seed, int32 Revision);

	UFUNCTION()
	void HandleMonsterDied(ATBMonster* Monster);

	UFUNCTION()
	void HandleMonsterDestroyed(AActor* Actor);

	void TrySpawn();
	bool FindSpawnTransform(FTransform& OutTransform) const;
	void RemoveMonster(ATBMonster* Monster);

	UPROPERTY(Transient)
	TObjectPtr<ACityLayoutCoordinator> City;
	TArray<TWeakObjectPtr<ATBMonster>> LivingMonsters;
	TArray<double> SpawnReadyTimes;
	FTimerHandle SpawnTimer;
	bool bCityReady = false;
	int32 ConsecutiveFailures = 0;
};
