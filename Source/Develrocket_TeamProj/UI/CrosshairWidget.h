#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CrosshairWidget.generated.h"

class UImage;
UCLASS()
class DEVELROCKET_TEAMPROJ_API UCrosshairWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void SetCrosshairSpread(float Spread);
	void SetAiming(bool bIsAiming);
	
protected:
	virtual void NativeConstruct() override;
	
	//조준선 : 위젯과 간격 설정
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> CrosshairTop;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> CrosshairBottom;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> CrosshairLeft;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> CrosshairRight;
	UPROPERTY(EditDefaultsOnly, Category="UI")
	float CrosshairMinGap = 8.f;
	UPROPERTY(EditDefaultsOnly, Category="UI")
	float CrosshairMaxGap = 40.f;
	
	//조준선의 이동을 위한 값
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	void ApplyCrosshairSpread(float Spread);
	float TargetSpread = 0.f;
	float CurrentSpread = 0.f;
	UPROPERTY(EditDefaultsOnly, Category="UI")
	float CrosshairInterpSpeed = 12.f;
};