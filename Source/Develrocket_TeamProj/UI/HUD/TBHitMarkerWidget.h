#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TBHitMarkerWidget.generated.h"

class UImage;
class UWidgetAnimation;
UCLASS()

class DEVELROCKET_TEAMPROJ_API UTBHitMarkerWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void SetShowHitMarker(bool bIsKill);
	
protected:
	//타격 피드백을 위한 값
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> HitMarker;
	UPROPERTY(Transient, meta=(BindWidgetAnim))
	TObjectPtr<UWidgetAnimation> HitMarkerAnim;
	UPROPERTY(EditDefaultsOnly, Category="UI")
	FLinearColor HitColor = FLinearColor(0.85f, 0.81f, 0.75f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category="UI")
	FLinearColor KillColor = FLinearColor(0.7f, 0.13f, 0.23f, 1.f);
};