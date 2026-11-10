#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TBInteractable.generated.h"

class UAnimMontage;

UINTERFACE(BlueprintType)
class DEVELROCKET_TEAMPROJ_API UTBInteractable : public UInterface
{
	GENERATED_BODY()
};

class DEVELROCKET_TEAMPROJ_API ITBInteractable
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
	
	// 상호작용할 때 플레이어가 재생할 3인칭 몽타주 (없으면 GA가 실행되지 않음)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction|Animation")
	UAnimMontage* GetInteractMontage() const;
	
	// 1인칭 몽타주 (없으면 nullptr)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction|Animation")
	UAnimMontage* GetInteractFirstPersonMontage() const;
	
	// 결과 시점
	
	// true : 몽타주 시작 시 결과(대화) / false : 몽타주가 끝나야 결과(흡혈)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction")
	bool ShouldResolveOnStart() const;
};