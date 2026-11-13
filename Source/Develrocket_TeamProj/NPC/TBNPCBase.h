#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/TBInteractable.h"
#include "TBNPCBase.generated.h"

class UCapsuleComponent;
class USkeletalMeshComponent;
class UAnimMontage;

UCLASS(Abstract)
class DEVELROCKET_TEAMPROJ_API ATBNPCBase : public AActor, public ITBInteractable
{
	GENERATED_BODY()

public:
	ATBNPCBase();

	// 상호작용 계약 기본 구현 ----------------------

	virtual bool CanInteract_Implementation(AActor* InteractInstigator) const override;
	virtual void Interact_Implementation(AActor* InteractInstigator) override;
	virtual void InteractLocal_Implementation(AActor* InteractInstigator) override;
	virtual FText GetInteractPrompt_Implementation() const override;
	virtual UAnimMontage* GetInteractMontage_Implementation() const override;
	virtual UAnimMontage* GetInteractFirstPersonMontage_Implementation() const override;
	virtual bool ShouldResolveOnStart_Implementation() const override;

protected:
	// 컴포넌트 ----------------------

	// 탐지·충돌 전담 (루트)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCapsuleComponent> Capsule;

	// 외형 전담 (충돌 없음)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> Mesh;

	// 상호작용 데이터 ----------------------

	// false면 탐지 대상에서 제외
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bCanInteract = true;

	// HUD에 띄울 문구
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText InteractPrompt;

	// 3인칭 몽타주 (없으면 몽타주 없이 즉시 결과 처리)
	UPROPERTY(EditDefaultsOnly, Category = "Interaction|Animation")
	TObjectPtr<UAnimMontage> InteractMontage;

	// 1인칭 몽타주
	UPROPERTY(EditDefaultsOnly, Category = "Interaction|Animation")
	TObjectPtr<UAnimMontage> InteractFirstPersonMontage;
	
	// NPC 반응 ----------------------

	// 상호작용당했을 때 NPC 자신이 재생할 몽타주 (비우면 반응 없음)
	// 위의 InteractMontage들은 "플레이어"가 재생하는 것이고, 이건 "NPC"가 재생
	UPROPERTY(EditDefaultsOnly, Category = "Interaction|Reaction")
	TObjectPtr<UAnimMontage> ReactMontage;

	// [로컬] 이 머신에서 NPC 메시에 반응 몽타주 재생
	void PlayReactMontage();
};