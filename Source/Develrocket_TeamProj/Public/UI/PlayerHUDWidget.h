// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerHUDWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UImage;
class UWidgetAnimation;
class UTexture2D;
class UOverlay;

/**
 *
 */
UCLASS()
class DEVELROCKET_TEAMPROJ_API UPlayerHUDWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void SetHealth(float CurrentHealth, float MaxHealth);
	void SetAmmo(int32 CurrentAmmo, int32 ReserveAmmo);
	void SetCrosshairSpread(float Spread);
	void SetAiming(bool bIsAiming);
	
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> AmmoText;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UProgressBar> HealthBar;
	
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
	
	//조준선 : 위젯과 간격 설정
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UOverlay> Crosshair;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> CrosshairCenter;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> CrosshairTop;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> CrosshairBottom;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> CrosshairLeft;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> CrosshairRight;
	UPROPERTY(EditDefaultsOnly, Category="UI")
	float CrosshairMinGap = 8.f;
	UPROPERTY(EditDefaultsOnly, Category="UI")
	float CrosshairMaxGap = 40.f;
	
	//조준선의 이동을 위한 값
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	void ApplyCrosshairSpread(float Spread);
	float TargetSpread = 0.f;
	float CurrentSpread = 0.f;
	UPROPERTY(EditDefaultsOnly, Category="UI")
	float CrosshairInterpSpeed = 12.f;
};
