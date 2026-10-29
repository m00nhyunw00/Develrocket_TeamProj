#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TBHealthBarWidget.generated.h"

class UProgressBar;
UCLASS()

class DEVELROCKET_TEAMPROJ_API UTBHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void SetHealth(float CurrentHealth, float MaxHealth);
	
protected:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UProgressBar> HealthBar;
};