#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TBAmmoWidget.generated.h"

class UTextBlock;

UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBAmmoWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void SetAmmo(int32 CurrentAmmo, int32 ReserveAmmo, int32 MagazineSize);
	
protected:
	//재장전 피드백을 위한 값
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> ReloadText;
	UPROPERTY(EditDefaultsOnly, Category="UI")
	float LowAmmoRatio = 0.3f;
	UPROPERTY(EditDefaultsOnly, Category="UI")
	FLinearColor AmmoNormalColor = FLinearColor(0.85f, 0.81f, 0.75f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category="UI")
	FLinearColor AmmoLowColor = FLinearColor(0.70f, 0.13f, 0.23f, 1.f);
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> AmmoText;
};
