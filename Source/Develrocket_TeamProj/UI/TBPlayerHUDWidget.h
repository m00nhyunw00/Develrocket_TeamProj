#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TBPlayerHUDWidget.generated.h"

class UTBAmmoWidget;
class UTBHealthBarWidget;
class UTBCrosshairWidget;
class UTBHitMarkerWidget;
class UTBLowHealthOverlayWidget;
class UTBSkillBarWidget;

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
	void NotifyShotFired();
	void SetAiming(bool bIsAiming);
	void SetShowHitMarker(bool bIsKill);
	void SetAmmo(int32 CurrentAmmo, int32 AmmoAtLastReload, int32 MagazineSize, bool bCanReload);
	void SetSkill(int32 Index, UTexture2D* Icon, const FText& Key);
	void StartSkillCooldown(int32 Index, float Duration);
	
protected:
	//부품 조립을 위한 값
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTBAmmoWidget> Ammo;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTBHealthBarWidget> HealthBar;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTBCrosshairWidget> Crosshair;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTBHitMarkerWidget> HitMarker;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTBLowHealthOverlayWidget> LowHealthOverlay;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTBSkillBarWidget> SkillBar;
};
