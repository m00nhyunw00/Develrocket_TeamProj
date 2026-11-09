#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/TBInteractable.h"
#include "TBLoreNPC.generated.h"

class UCapsuleComponent;
class USkeletalMeshComponent;

/**
 * 대기실(L_MatchWait) 세계관 설명 NPC
 *
 * - 캡슐(루트): 상호작용 탐지와 충돌 전담 (Visibility Block)
 * - 메시: 외형만 담당 (충돌 없음)
 * - Interact      [서버]  : 지금은 바꿀 게임 상태 없음
 * - InteractLocal [클라]  : 돌아보기(4단계), 세계관 UI(5단계)가 붙을 자리
 *
 * 맵에 배치해서 사용 (배치 액터라 bReplicates 없이도 RPC로 참조 가능)
 */
UCLASS()
class DEVELROCKET_TEAMPROJ_API ATBLoreNPC : public AActor, public ITBInteractable
{
	GENERATED_BODY()

public:
	ATBLoreNPC();

	// 상호작용 계약 구현 ----------------------

	virtual bool CanInteract_Implementation(AActor* InteractInstigator) const override;
	virtual void Interact_Implementation(AActor* InteractInstigator) override;
	virtual void InteractLocal_Implementation(AActor* InteractInstigator) override;
	virtual FText GetInteractPrompt_Implementation() const override;

protected:
	// 컴포넌트 ----------------------

	// 탐지·충돌 전담 (루트)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCapsuleComponent> Capsule;

	// 외형 전담 (충돌 없음)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> Mesh;

	// 상호작용 데이터 ----------------------

	// false면 탐지 대상에서 제외 (대화 중 연타 방지 등)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bCanInteract = true;

	// HUD에 띄울 문구
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText InteractPrompt;
};