#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LowHealthOverlayWidget.generated.h"

class UImage;
class UWidgetAnimation;
class UTexture2D;
UCLASS()

class DEVELROCKET_TEAMPROJ_API ULowHealthOverlayWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void SetHealth(float CurrentHealth, float MaxHealth);
	
protected:
	//HUD의 심장박동을 위한 변수 값
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> BloodOverlay;
	UPROPERTY(Transient, meta=(BindWidgetAnim))
	TObjectPtr<UWidgetAnimation> HeartbeatAnim;
	UPROPERTY(EditDefaultsOnly, Category="UI")
	float LowHealthThreshold = 0.3f;
	
	//HUD 저체력일때 심장박동을 위한 함수
	void UpdateLowHealthEffect(float HealthPercent);
	
	//HUD 저체력시 핏빛 애니메이션을 위한 텍스처 배열
	UPROPERTY(EditDefaultsOnly, Category="UI")
	TArray<TObjectPtr<UTexture2D>> BloodStageTextures;
	int32 CurrentBloodStage = INDEX_NONE;
};