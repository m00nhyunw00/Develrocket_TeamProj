// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerHUDWidget.generated.h"

class UProgressBar;
class UTextBlock;

/**
 * 
 */
UCLASS()
class DEVELROCKET_TEAMPROJ_API UPlayerHUDWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> AmmoText;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UProgressBar> HealthBar;
};
