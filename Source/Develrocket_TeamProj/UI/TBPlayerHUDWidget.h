// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TBPlayerHUDWidget.generated.h"

class UTBAmmoWidget;
class UTBHealthBarWidget;
class UTBCrosshairWidget;
class UTBHitMarkerWidget;
class UTBLowHealthOverlayWidget;

/**
 *
 */
UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBPlayerHUDWidget : public UUserWidget
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
	TObjectPtr<UTBAmmoWidget> AmmoWidget;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTBHealthBarWidget> HealthBarWidget;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTBCrosshairWidget> CrosshairWidget;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTBHitMarkerWidget> HitMarkerWidget;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTBLowHealthOverlayWidget> LowHealthOverlayWidget;
	
};
