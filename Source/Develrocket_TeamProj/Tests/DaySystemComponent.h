#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "DaySystemComponent.generated.h"

class ADirectionalLight;

USTRUCT()
struct FDayCycleState
{
	GENERATED_BODY()

	UPROPERTY() FName LightName;
	UPROPERTY() FRotator InitialRotation = FRotator::ZeroRotator;
	UPROPERTY() float StartServerTime = 0.0f;
	UPROPERTY() float CycleSeconds = 60.0f;
	UPROPERTY() bool bInitialized = false;
};

// 서버에 하나만 생성되며 모든 클라이언트의 태양 방향을 동기화
UCLASS(NotBlueprintable, NotPlaceable, Transient)
class DEVELROCKET_TEAMPROJ_API ADayCycleController : public AActor
{
	GENERATED_BODY()

public:
	ADayCycleController();
	void InitializeCycle(ADirectionalLight* Light, float CycleSeconds);
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION() void OnRep_CycleState();
	void UpdateSunRotation();
	UPROPERTY(ReplicatedUsing=OnRep_CycleState) FDayCycleState CycleState;
	UPROPERTY(Transient) TObjectPtr<ADirectionalLight> LocalLight;
};

// 플레이어와 태양 사이의 차폐 여부를 서버에서 확인하는 테스트 컴포넌트
UCLASS(ClassGroup=(Test), meta=(BlueprintSpawnableComponent))
class DEVELROCKET_TEAMPROJ_API UDaySystemComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDaySystemComponent();

	// 서버에서 태양 액터를 지정하며, 소유 액터 자신은 충돌 검사에서 제외
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Day System")
	void SetDirectionalLight(ADirectionalLight* InLight);

	UFUNCTION(BlueprintPure, Category="Day System")
	bool IsSunTraceClear() const { return bSunTraceClear; }

	UFUNCTION(BlueprintPure, Category="Day System")
	bool HasValidSunTrace() const { return bHasValidSunTrace; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Day System")
	TObjectPtr<ADirectionalLight> DirectionalLight;

	UPROPERTY(EditAnywhere, Category="Day System", meta=(ClampMin="0.05", Units="s"))
	float TraceInterval = 0.2f;

	UPROPERTY(EditAnywhere, Category="Day System", meta=(ClampMin="1.0", Units="cm"))
	float TraceDistance = 100000.0f;

	UPROPERTY(EditAnywhere, Category="Day System")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, Category="Day System")
	FVector TraceStartOffset = FVector(0.0, 0.0, 50.0);

	UPROPERTY(EditAnywhere, Category="Day System|Debug")
	bool bDrawDebugTrace = true;

	// 현실 시간 기준 태양 한 바퀴에 걸리는 시간
	UPROPERTY(EditDefaultsOnly, Category="Day System|Sun", meta=(ClampMin="1.0", Units="s"))
	float FullCycleSeconds = 60.0f;

private:
	void TraceSunlight();
	void EnsureDayCycleController();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastDrawTrace(FVector Start, FVector End, bool bClear);

	UPROPERTY(Replicated)
	bool bSunTraceClear = false;

	UPROPERTY(Replicated)
	bool bHasValidSunTrace = false;

	FTimerHandle TraceTimer;
};
