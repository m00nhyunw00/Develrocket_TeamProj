// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TBQuickSlotWidget.generated.h"

class UTextBlock;
class UImage;
class UTexture2D;

/**
 * 
 */
UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBQuickSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetItem(UTexture2D* Icon, int32 Count);
	void SetSelected(bool bIsSelected);
	
protected:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> SlotFrame;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> ItemIcon;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> CountText;
	UPROPERTY(EditDefaultsOnly, Category="UI")
	TObjectPtr<UTexture2D> NormalFrame;
	UPROPERTY(EditDefaultsOnly, Category="UI")
	TObjectPtr<UTexture2D> SelectedFrame;
};
