#pragma once

#include "CoreMinimal.h"
#include "Character/Player/TBPlayer.h"
#include "TBAmanai.generated.h"

UCLASS()
class DEVELROCKET_TEAMPROJ_API ATBAmanai : public ATBPlayer
{
	GENERATED_BODY()

public:
	ATBAmanai(const FObjectInitializer& ObjectInitializer);

	virtual void BeginMeleeTrace(USkeletalMeshComponent* SourceMesh) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 질풍참 이동 중 Pawn 캡슐 통과 여부 변경
	void SetDashPawnPassThrough(bool bEnabled);

private:
	// 다른 플레이어 화면의 아마나이 캡슐에도 통과 상태 반영
	UPROPERTY(ReplicatedUsing = OnRep_DashPawnPassThrough)
	bool bDashPawnPassThrough = false;

	UFUNCTION()
	void OnRep_DashPawnPassThrough();

	// 돌진 시작 전 이 캐릭터가 실제 사용하던 Pawn 충돌 응답
	ECollisionResponse SavedPawnResponse = ECR_Block;
	bool bHasSavedPawnResponse = false;

	void TryRestoreDashPawnCollision();
};