#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TBInteractionComponent.generated.h"

/**
 * 플레이어 상호작용 탐지 컴포넌트
 *
 * [로컬]   0.1초마다 시야 원뿔 안의 상호작용 대상을 찾아 FocusedActor에 저장 (HUD 표시용)
 * [서버]   GA_Interact 실행 시 서버가 같은 규칙으로 다시 탐지해 대상을 검증 (치트 방지)
 * [클라]   서버 처리 후 Client RPC로 상호작용한 플레이어에게만 로컬 반응(UI/연출) 통지
 *
 * 판정 규칙은 FindBestTarget 하나로 통일해서 클라이언트와 서버가 같은 기준을 사용함
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnFocusedActorChanged, AActor* /*NewFocus*/);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class DEVELROCKET_TEAMPROJ_API UTBInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTBInteractionComponent();

	// [로컬] 지금 바라보는 상호작용 대상 (HUD 프롬프트용, 없으면 nullptr)
	AActor* GetFocusedActor() const { return FocusedActor.Get(); }

	// [서버] 서버가 직접 다시 탐지한 상호작용 대상 (GA에서 검증용으로 사용)
	AActor* FindTargetForServer() const;

	// [서버 → 소유 클라이언트] 상호작용이 처리되었음을 알리고 대상의 InteractLocal 실행
	UFUNCTION(Client, Reliable)
	void Client_NotifyInteracted(AActor* Target);
	
	FOnFocusedActorChanged OnFocusedActorChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 탐지 설정 ----------------------

	// 눈 위치에서 탐지할 거리
	UPROPERTY(EditDefaultsOnly, Category = "Interaction", meta = (ClampMin = "0.0", Units = "cm"))
	float InteractRange = 300.0f;

	// 시야 원뿔 반각: 시선 기준 이 각도 안의 대상만 후보
	UPROPERTY(EditDefaultsOnly, Category = "Interaction", meta = (ClampMin = "1.0", ClampMax = "90.0", Units = "deg"))
	float ViewHalfAngle = 30.0f;

	// 로컬 탐지 주기
	UPROPERTY(EditDefaultsOnly, Category = "Interaction", meta = (ClampMin = "0.01", Units = "s"))
	float ScanInterval = 0.1f;

	// 디버그 ----------------------

	// 탐지 범위 표시 (확인 끝나면 BP_TBPlayer에서 끄기)
	UPROPERTY(EditDefaultsOnly, Category = "Interaction|Debug")
	bool bDrawDebug = true;

private:
	// [로컬] 타이머로 반복 호출되는 포커스 갱신
	void ScanForTarget();

	// [공용] 판정 규칙: 반경 → 인터페이스/CanInteract → 시야 원뿔 → 벽 가림 → 가장 정면
	AActor* FindBestTarget(APawn* OwnerPawn, const FVector& ViewLocation, const FVector& ViewDir) const;

	// [로컬] 탐지 결과 디버그 표시
	void DrawScanDebug(const FVector& ViewLocation, const FVector& ViewDir, const AActor* Target) const;

	// 약한 참조: 대상이 파괴되면 자동으로 null
	TWeakObjectPtr<AActor> FocusedActor;

	FTimerHandle ScanTimerHandle;
};