#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "TBInteractInterface.generated.h"

class UAnimMontage;

UINTERFACE(BlueprintType)
class DEVELROCKET_TEAMPROJ_API UTBInteractInterface : public UInterface
{
	GENERATED_BODY()
};

class DEVELROCKET_TEAMPROJ_API ITBInteractInterface
{
	GENERATED_BODY()

public:
	// 판정 --------------------
	
	// 지금 상호작용 가능한 상태인지 (대화 중이면 false 등)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool CanInteract(AActor* InteractInstigator) const;
	
	// 결과 ----------------------

	// [서버] 실제 상호작용: 게임 상태를 바꾸는 일 (아이템 획득, 문 열기 등)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void Interact(AActor* InteractInstigator);

	// [상호작용한 플레이어의 클라이언트] 그 사람에게만 보이는 반응 (UI, 연출)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void InteractLocal(AActor* InteractInstigator);
	
	// 표시 -------------------------

	// HUD에 띄울 문구 
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FText GetInteractPrompt() const;
	
	// 연출 -------------------------
	
	// 상호작용할 때 플레이어가 재생할 선택적 3인칭 몽타주
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction|Animation")
	UAnimMontage* GetInteractMontage() const;
	
	// 1인칭 몽타주 (없으면 nullptr)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction|Animation")
	UAnimMontage* GetInteractFirstPersonMontage() const;
	
	// 진행 알림 --------------------
	// [서버] 몽타주가 시작될 때 대상에게 알림. 흡혈 대상이 멈추는 용도
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void BeginInteract(AActor* InteractInstigator);
	
	// [서버] 결과가 나기 전에 상호작용이 끊겼을 때 알림 (몽타주 중단, 어빌리티 취소)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void CancelInteract(AActor* InteractInstigator);
	
	// 이 대상과 상호작용할 때 플레이어가 실행할 어빌리티 태그
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FGameplayTag GetInteractionAbilityTag(AActor* InteractInstigator) const;
};
