// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerHUDWidget.generated.h"

class UImage;
class UWidgetAnimation;
class UTexture2D;
class UAmmoWidget;
class UHealthBarWidget;
class UCrosshairWidget;
class UHitMarkerWidget;

/**
 *
 */
UCLASS()
class DEVELROCKET_TEAMPROJ_API UPlayerHUDWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void SetHealth(float CurrentHealth, float MaxHealth);
	void SetCrosshairSpread(float Spread);
	void SetAiming(bool bIsAiming);
	void SetShowHitMarker(bool bIsKill);
	void SetAmmo(int32 CurrentAmmo, int32 ReserveAmmo, int32 MagazineSize);
	
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
	
	//부품 조립을 위한 값
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UAmmoWidget> AmmoWidget;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UHealthBarWidget> HealthBarWidget;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UCrosshairWidget> CrosshairWidget;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UHitMarkerWidget> HitMarkerWidget;
	
};
