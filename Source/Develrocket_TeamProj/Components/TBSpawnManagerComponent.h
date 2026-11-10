#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TBSpawnManagerComponent.generated.h"

class ATBLevelManager;
class ATBMonster;
class ACityLayoutCoordinator;
struct FTBSpawnPoolConfig;

UCLASS(ClassGroup=(Spawn))
class DEVELROCKET_TEAMPROJ_API UTBSpawnManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTBSpawnManagerComponent();
	// 도시 생성 완료 후 서버에서 호출하여 풀별 스폰 검사 타이머를 시작합니다.
	void StartSpawning();

	// 사용 중인 액터를 비활성화해 원래 풀의 재사용 대기 목록으로 옮깁니다.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Spawn|Pool")
	bool ReturnToPool(AActor* Actor);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// 몬스터 사망 이벤트를 받으면 일정 시간 후 풀 반환을 예약합니다.
	UFUNCTION()
	void HandleMonsterDied(ATBMonster* Monster);

	// 풀별 수량과 다음 시도 시각을 확인하고 재사용하거나 새로 생성합니다.
	void UpdatePools();
	// 몬스터는 바닥과 NavMesh를, 다른 액터는 태그가 맞는 TargetPoint를 검사합니다.
	bool FindSpawnTransform(const FTBSpawnPoolConfig& Entry, FTransform& OutTransform) const;

	// 런타임 풀 데이터를 소유한 레벨 매니저를 참조합니다.
	UPROPERTY(Transient)
	TObjectPtr<ATBLevelManager> PoolOwner;
	UPROPERTY(Transient)
	TObjectPtr<ACityLayoutCoordinator> City;
	// 런타임 풀의 인덱스별 다음 스폰 시도 시각입니다.
	TArray<double> NextSpawnTimes;
	FTimerHandle UpdateTimer;
};
