#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TBRotationComponent.generated.h"

// 지정 대상 방향으로 소유 액터의 수평 회전 제어
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class DEVELROCKET_TEAMPROJ_API UTBRotationComponent : public UActorComponent
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, BLueprintReadWrite)
	AActor* TargetActor;

	// 대상 지정과 별도로 전환하는 회전 허용 상태
	UPROPERTY(EditAnywhere, BLueprintReadWrite)
	bool bShouldRotate = false;

	// 초당 회전 각도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation")
	float RotationSpeed = 1000.f;

public:
	UTBRotationComponent();

public:
	FORCEINLINE void SetTargetActor(AActor* InActor) { TargetActor = InActor; }
	FORCEINLINE void ToggleShouldRotate(const bool bRotate) { bShouldRotate = bRotate; }

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
