// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "PlayerHUD.generated.h"

class UPlayerHUDWidget;

/**
 * 
 */
UCLASS()
class DEVELROCKET_TEAMPROJ_API APlayerHUD : public AHUD
{
	GENERATED_BODY()
	
protected:
	virtual void BeginPlay() override;
	
	UPROPERTY(EditDefaultsOnly, Category="UI")
	TSubclassOf<UPlayerHUDWidget> HUDWidgetClass;
	
	UPROPERTY()
	TObjectPtr<UPlayerHUDWidget> HUDWidget;
};
