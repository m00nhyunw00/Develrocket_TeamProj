#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TBHealthBarWidget.generated.h"

class UProgressBar;
class UCanvasPanel;
class UTexture2D;
UCLASS()

class DEVELROCKET_TEAMPROJ_API UTBHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void SetHealth(float CurrentHealth, float MaxHealth);
	
protected:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UProgressBar> HealthBar;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UCanvasPanel> TickLayer;
	
	UPROPERTY(EditDefaultsOnly, Category = "HealthTick")
	TObjectPtr<UTexture2D> TickTexture;
	
	UPROPERTY(EditDefaultsOnly, Category = "HealthTick")
	float TickInterval = 20.0f;
	
private:
	void RebuildTicks(float MaxHealth);
	
	static constexpr float FillStart = 176.0f / 1600.0f;
	static constexpr float FillEnd = 1425.0f / 1600.0f;
	
	static constexpr float TickHalfWidth = 14.0f / 1600.0f;
	static constexpr float TickTop = 156.0f / 400.0f;
	static constexpr float TickBottom = 244.0f / 400.0f;
	
	float CachedMaxHealth = -1.0f;
};