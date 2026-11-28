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
class UTBInteractPromptWidget;
class UTBDamageIndicatorWidget;
class UFont;
class UTextBlock;

/**
 *
 */
UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBPlayerHUDWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UTBPlayerHUDWidget(const FObjectInitializer& ObjectInitializer);

	void ShowPowerPanel();
	void SetPowerDamageMultiplier(float DamageMultiplier);
	void SetHealth(float CurrentHealth, float MaxHealth);
	void SetCrosshairSpread(float Spread);
	void NotifyShotFired();
	void SetAiming(bool bIsAiming);
	void SetShowHitMarker(bool bIsKill);
	void SetAmmo(int32 CurrentAmmo, int32 AmmoAtLastReload, int32 MagazineSize, bool bCanReload);
	void SetSkill(int32 Index, UTexture2D* Icon, const FText& Key);
	void SetSkillCooldownRemaining(int32 Index, float RemainingSeconds);
	void ShowInteractPrompt(const FText& Key, const FText& Prompt);
	void HideInteractPrompt();
	void AddDamageIndicator(const FVector& Origin, AActor* Source);

protected:
	//부품 조립을 위한 값
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTBAmmoWidget> Ammo;
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTBHealthBarWidget> HealthBar;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTBCrosshairWidget> Crosshair;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTBHitMarkerWidget> HitMarker;
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTBLowHealthOverlayWidget> LowHealthOverlay;
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTBSkillBarWidget> SkillBar;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTBInteractPromptWidget> InteractPrompt;
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTBDamageIndicatorWidget> DamageIndicator;

private:
	UPROPERTY()
	TObjectPtr<UTexture2D> PowerPanelTexture;

	UPROPERTY()
	TObjectPtr<UFont> PowerPanelFont;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PowerDamageText;

};
