#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TBInteractable.generated.h"

UINTERFACE(BlueprintType)
class DEVELROCKET_TEAMPROJ_API UTBInteractable : public UInterface
{
	GENERATED_BODY()
};

class DEVELROCKET_TEAMPROJ_API ITBInteractable
{
	GENERATED_BODY()

	public:
	// 지금 상호작용 가능한 상태인지 (대화 중이면 false 등)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool CanInteract(AActor* InteractInstigator) const;

	// 실제 상호작용 실행
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void Interact(AActor* InteractInstigator);

	// HUD에 띄울 문구 
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FText GetInteractPrompt() const;
};