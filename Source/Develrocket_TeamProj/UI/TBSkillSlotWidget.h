#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TBSkillSlotWidget.generated.h"

class UTexture2D;
class UImage;
class UTextBlock;
UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBSkillSlotWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void SetIcon(UTexture2D* Icon);
	void SetKey(const FText& Key);
	void StartCooldown(float Duration);
	
protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeConstruct() override;
	virtual void UpdateReadyVisual(bool bReady);
	
	// 화면 부품
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> SkillFrame;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> SkillIcon;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> CooldownText;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> KeyText;
	
	// 프레임 텍스처
	UPROPERTY(EditDefaultsOnly, Category="UI")
	TObjectPtr<UTexture2D> CooldownFrame;
	UPROPERTY(EditDefaultsOnly, Category="UI")
	TObjectPtr<UTexture2D> ReadyFrame;
	
	float RemainingTime = 0.f;
	bool bIsSkillReady = true;
	
};