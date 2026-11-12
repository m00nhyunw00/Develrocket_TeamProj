#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TBSpawnPoolData.generated.h"

// 데이터 에셋에서 편집하며, 도시 생성 완료 시 런타임 풀에 복사할 스폰 설정입니다.
USTRUCT(BlueprintType)
struct FTBSpawnPoolConfig
{
	GENERATED_BODY()

	// 생성할 액터 클래스. 풀 재사용을 위해 TBPoolable 인터페이스가 필요합니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pool")
	TSubclassOf<AActor> ActorClass;

	// 이 항목에서 동시에 활성화할 수 있는 최대 액터 수입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pool", meta=(ClampMin="0"))
	int32 MaxActiveCount = 5;

	// 활성화 및 대기 중인 액터를 합친 최대 보유 수입니다. 미리 생성하는 수는 아닙니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pool", meta=(ClampMin="0"))
	int32 PoolSize = 5;

	// 스폰 시도 간격입니다. 위치 탐색에 실패해도 이 간격 후 다시 시도합니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pool", meta=(ClampMin="0.1", Units="s"))
	float SpawnInterval = 1.0f;

	// Time between a death notification and returning the actor to the pool.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pool", meta=(ClampMin="0.0", Units="s"))
	float ReturnDelay = 1.0f;

	// Use the actor's default capsule for spawn clearance.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pool|Spawn")
	bool bUseActorCapsule = true;

	// Used for spawn clearance when the actor has no native capsule component.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pool|Spawn", meta=(EditCondition="!bUseActorCapsule", ClampMin="1.0", Units="cm"))
	float SpawnClearanceRadius = 34.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pool|Spawn", meta=(EditCondition="!bUseActorCapsule", ClampMin="1.0", Units="cm"))
	float SpawnClearanceHalfHeight = 88.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pool|Spawn", meta=(EditCondition="!bUseActorCapsule"))
	FName SpawnCollisionProfile = TEXT("Pawn");
};

// 레벨 매니저가 매치별로 소유하는 풀 상태입니다. 에셋 자체에는 액터 목록을 저장하지 않습니다.
USTRUCT()
struct FTBSpawnPoolRuntime
{
	GENERATED_BODY()

	// 초기화 시 데이터 에셋 또는 기존 레벨 설정에서 복사한 값입니다.
	UPROPERTY(Transient)
	FTBSpawnPoolConfig Config;

	// 현재 활성 상태로 관리 중인 액터입니다.
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> ActiveActors;

	// 풀에 반환되어 재사용을 기다리는 액터입니다.
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> AvailableActors;
};

// 이전 레벨 인스턴스 설정을 보존하기 위한 구조체입니다. 데이터 에셋이 없을 때 사용합니다.
USTRUCT(BlueprintType)
struct FTBSpawnPoolEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pool")
	TSubclassOf<AActor> ActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pool", meta=(ClampMin="0"))
	int32 MaxActiveCount = 5;

	// 활성화 및 대기 중인 액터를 합친 최대 보유 수입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pool", meta=(ClampMin="0"))
	int32 PoolSize = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pool", meta=(ClampMin="0.1", Units="s"))
	float SpawnInterval = 1.0f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> ActiveActors;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> AvailableActors;
};
