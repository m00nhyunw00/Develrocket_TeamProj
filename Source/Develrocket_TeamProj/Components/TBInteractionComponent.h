#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TBInteractionComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class DEVELROCKET_TEAMPROJ_API UTBInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTBInteractionComponent();

	// GA가 물어볼 함수: 지금 바라보는 상호작용 대상 (없으면 nullptr)
	AActor* GetFocusedActor() const { return FocusedActor.Get(); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 탐지 설정 ----------------------

	// 카메라에서 탐지할 거리
	UPROPERTY(EditDefaultsOnly, Category = "Interaction", meta = (ClampMin = "0.0", Units = "cm"))
	float InteractRange = 300.0f;

	// 시선 빔의 굵기(구 반지름): 클수록 대충 바라봐도 잡힘
	UPROPERTY(EditDefaultsOnly, Category = "Interaction", meta = (ClampMin = "0.0", Units = "cm"))
	float TraceRadius = 25.0f;

	// 탐지 주기
	UPROPERTY(EditDefaultsOnly, Category = "Interaction", meta = (ClampMin = "0.01", Units = "s"))
	float ScanInterval = 0.1f;

	// 디버그 ----------------------

	// 탐지 범위 표시 (확인 끝나면 BP_TBPlayer에서 끄기)
	UPROPERTY(EditDefaultsOnly, Category = "Interaction|Debug")
	bool bDrawDebug = true;

private:
	// 시선 앞 대상 탐지 (타이머로 반복 호출)
	void ScanForTarget();

	// 탐지 결과 디버그 표시
	void DrawScanDebug(const FVector& Start, const FVector& End, const FHitResult& Hit, bool bHasTarget) const;

	// 약한 참조: 대상이 파괴되면 자동으로 null
	TWeakObjectPtr<AActor> FocusedActor;

	FTimerHandle ScanTimerHandle;
};