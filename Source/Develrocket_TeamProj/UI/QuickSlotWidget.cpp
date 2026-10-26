// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/QuickSlotWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

void UQuickSlotWidget::SetItem(UTexture2D* Icon, int32 Count)
{
	if (!ItemIcon || !CountText)
	{
		return;
	}

	// 아이콘 그림 교체
	if (Icon)
	{
		ItemIcon->SetBrushFromTexture(Icon);
	}

	// 개수 표시 "×2"
	const FString CountString = FString::Printf(TEXT("×%d"), Count);
	CountText->SetText(FText::FromString(CountString));

	// 0개면 어둡게
	const float Opacity = (Count > 0) ? 1.f : 0.35f;
	ItemIcon->SetRenderOpacity(Opacity);
	CountText->SetRenderOpacity(Opacity);
}

void UQuickSlotWidget::SetSelected(bool bIsSelected)
{
	if (!SlotFrame)
	{
		return;
	}

	UTexture2D* Frame = bIsSelected ? SelectedFrame.Get() : NormalFrame.Get();
	if (Frame)
	{
		SlotFrame->SetBrushFromTexture(Frame);
	}
}
