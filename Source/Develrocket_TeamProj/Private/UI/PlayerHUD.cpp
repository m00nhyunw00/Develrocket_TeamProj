// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/PlayerHUD.h"

#include "Blueprint/UserWidget.h"
#include "UI/PlayerHUDWidget.h"



void APlayerHUD::BeginPlay()
{
	Super::BeginPlay();
	
	if (!HUDWidgetClass)
	{
		return;
	}
	
	HUDWidget = CreateWidget<UPlayerHUDWidget>(GetOwningPlayerController(), HUDWidgetClass);
	if (HUDWidget)
	{
		HUDWidget->AddToViewport();
	}
}
