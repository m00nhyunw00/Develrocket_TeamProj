#include "UI/HUD/TBPlayerHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "UI/HUD/TBSkillBarWidget.h"
#include "UI/HUD/TBLowHealthOverlayWidget.h"
#include "UI/HUD/TBCrosshairWidget.h"
#include "UI/HUD/TBHealthBarWidget.h"
#include "UI/HUD/TBAmmoWidget.h"
#include "UI/HUD/TBHitMarkerWidget.h"
#include "UI/Interact/TBInteractPromptWidget.h"
#include "UI/HUD/TBDamageIndicatorWidget.h"
#include "UObject/ConstructorHelpers.h"

UTBPlayerHUDWidget::UTBPlayerHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static const ConstructorHelpers::FObjectFinder<UTexture2D> Texture(
		TEXT("/Game/_Game/Blueprints/UI/Textures/T_PowerHUD_Panel.T_PowerHUD_Panel"));
	static const ConstructorHelpers::FObjectFinder<UFont> Font(
		TEXT("/Game/_Game/Blueprints/UI/Fonts/NotoSerifKR-Bold_Font.NotoSerifKR-Bold_Font"));
	PowerPanelTexture = Texture.Object;
	PowerPanelFont = Font.Object;
}

void UTBPlayerHUDWidget::ShowPowerPanel()
{
	if (PowerDamageText || !WidgetTree || !PowerPanelTexture)
	{
		return;
	}

	const UCanvasPanelSlot* SkillSlot = SkillBar ? Cast<UCanvasPanelSlot>(SkillBar->Slot) : nullptr;
	UCanvasPanel* Canvas = SkillSlot ? Cast<UCanvasPanel>(SkillBar->GetParent()) : Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!Canvas)
	{
		return;
	}

	UCanvasPanel* Panel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PowerPanel"));
	Panel->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(Panel);
	PanelSlot->SetSize(FVector2D(310.0f, 103.0f));
	if (SkillSlot && SkillSlot->GetAnchors().Minimum == SkillSlot->GetAnchors().Maximum)
	{
		SkillBar->ForceLayoutPrepass();
		const FVector2D SkillSize = SkillSlot->GetAutoSize() ? SkillBar->GetDesiredSize() : SkillSlot->GetSize();
		const FVector2D SkillAlignment = SkillSlot->GetAlignment();
		PanelSlot->SetAnchors(SkillSlot->GetAnchors());
		PanelSlot->SetAlignment(FVector2D(1.0f, 0.5f));
		PanelSlot->SetPosition(SkillSlot->GetPosition() + FVector2D(
			-SkillSize.X * SkillAlignment.X - 10.0f, SkillSize.Y * (0.5f - SkillAlignment.Y)));
		PanelSlot->SetZOrder(SkillSlot->GetZOrder());
	}
	else
	{
		PanelSlot->SetAnchors(FAnchors(1.0f, 1.0f));
		PanelSlot->SetAlignment(FVector2D(1.0f, 1.0f));
		PanelSlot->SetPosition(FVector2D(-210.0f, -16.0f));
	}

	UImage* Background = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PowerPanelBackground"));
	Background->SetBrushFromTexture(PowerPanelTexture);
	UCanvasPanelSlot* BackgroundSlot = Panel->AddChildToCanvas(Background);
	BackgroundSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	BackgroundSlot->SetOffsets(FMargin(0.0f));

	auto AddPowerLabel = [this, Panel](FName Name, float Y, float Size, const FLinearColor& Color)
	{
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Label->GetFont();
		if (PowerPanelFont)
		{
			Font.FontObject = PowerPanelFont;
			Font.TypefaceFontName = NAME_None;
		}
		Font.Size = Size;
		Label->SetFont(Font);
		Label->SetColorAndOpacity(FSlateColor(Color));
		UCanvasPanelSlot* LabelSlot = Panel->AddChildToCanvas(Label);
		LabelSlot->SetPosition(FVector2D(83.0f, Y));
		LabelSlot->SetAutoSize(true);
		return Label;
	};

	PowerDamageText = AddPowerLabel(TEXT("PowerDamageText"), 25.0f, 25.0f, FLinearColor(0.93f, 0.76f, 0.43f));
	PowerPointsText = AddPowerLabel(TEXT("PowerPointsText"), 53.0f, 18.0f, FLinearColor(0.91f, 0.87f, 0.77f));
	SetPower(0.0f, 1.0f);
}

void UTBPlayerHUDWidget::SetPower(float Points, float DamageMultiplier)
{
	if (!PowerDamageText || !PowerPointsText)
	{
		return;
	}

	const FNumberFormattingOptions NumberOptions = FNumberFormattingOptions()
		.SetMinimumFractionalDigits(1)
		.SetMaximumFractionalDigits(1);
	PowerDamageText->SetText(FText::Format(
		NSLOCTEXT("TBPowerHUD", "DamageBonus", "피해 +{0}%"),
		FText::AsNumber((DamageMultiplier - 1.0f) * 100.0f, &NumberOptions)));
	PowerPointsText->SetText(FText::Format(
		NSLOCTEXT("TBPowerHUD", "PowerPoints", "혈력 {0}"),
		FText::AsNumber(Points, &NumberOptions)));
}

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

void UTBPlayerHUDWidget::AddDamageIndicator(const FVector& Origin, AActor* Source)
{
	if (DamageIndicator)
	{
		DamageIndicator->AddIndicator(Origin, Source);
	}
}



