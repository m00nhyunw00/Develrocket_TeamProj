#include "UI/HUD/TBPlayerHUDWidget.h"

#include "UI/HUD/TBSkillBarWidget.h"
#include "UI/HUD/TBLowHealthOverlayWidget.h"
#include "UI/HUD/TBCrosshairWidget.h"
#include "UI/HUD/TBHealthBarWidget.h"
#include "UI/HUD/TBAmmoWidget.h"
#include "UI/HUD/TBHitMarkerWidget.h"
#include "UI/Interact/TBInteractPromptWidget.h"

void UTBPlayerHUDWidget::SetHealth(float CurrentHealth, float MaxHealth)
{
	if (HealthBar)
	{
		HealthBar->SetHealth(CurrentHealth, MaxHealth);
	}
	
	if (LowHealthOverlay)
	{
		LowHealthOverlay->SetHealth(CurrentHealth, MaxHealth);
	}	
}

void UTBPlayerHUDWidget::SetCrosshairSpread(float Spread)
{
	if (!Crosshair)
	{
		return;
	}
	Crosshair->SetCrosshairSpread(Spread);		
}

void UTBPlayerHUDWidget::NotifyShotFired()
{
	if (Crosshair)
	{
		Crosshair->NotifyShotFired();
	}
}

void UTBPlayerHUDWidget::SetAiming(bool bIsAiming)
{
	if (!Crosshair)
	{
		return;
	}
	Crosshair->SetAiming(bIsAiming);
}

void UTBPlayerHUDWidget::SetShowHitMarker(bool bIsKill)
{
	if (!HitMarker)
	{
		return;
	}
	HitMarker->SetShowHitMarker(bIsKill);
}

void UTBPlayerHUDWidget::SetAmmo(int32 CurrentAmmo, int32 AmmoAtLastReload, int32 MagazineSize, bool bCanReload)
{
	if (Ammo)
	{
		Ammo->SetAmmo(CurrentAmmo, AmmoAtLastReload, MagazineSize, bCanReload);
	}
}

void UTBPlayerHUDWidget::SetSkill(int32 Index, UTexture2D* Icon, const FText& Key)
{
	if (!SkillBar)
	{
		return;
	}
	
	SkillBar->SetSkill(Index, Icon, Key);
}

void UTBPlayerHUDWidget::SetSkillCooldownRemaining(int32 Index, float RemainingSeconds)
{
	if (!SkillBar)
	{
		return;
	}
	SkillBar->SetSkillCooldownRemaining(Index, RemainingSeconds);
}

void UTBPlayerHUDWidget::ShowInteractPrompt(const FText& Key, const FText& Prompt)
{
	if (InteractPrompt)
	{
		InteractPrompt->ShowPrompt(Key, Prompt);
	}
}

void UTBPlayerHUDWidget::HideInteractPrompt()
{
	if (InteractPrompt)
	{
		InteractPrompt->HidePrompt();
	}
}



