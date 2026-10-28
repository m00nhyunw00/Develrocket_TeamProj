// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerHUDWidget.generated.h"

class UAmmoWidget;
class UHealthBarWidget;
class UCrosshairWidget;
class UHitMarkerWidget;
class ULowHealthOverlayWidget;

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
	//부품 조립을 위한 값
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UAmmoWidget> AmmoWidget;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UHealthBarWidget> HealthBarWidget;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UCrosshairWidget> CrosshairWidget;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UHitMarkerWidget> HitMarkerWidget;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<ULowHealthOverlayWidget> LowHealthOverlayWidget;
	
};
