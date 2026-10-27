// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerHUDWidget.generated.h"

class UImage;
class UWidgetAnimation;
class UTexture2D;
class UOverlay;
class UAmmoWidget;
class UHealthBarWidget;

/**
 *
 */
UCLASS()
class DEVELROCKET_TEAMPROJ_API UPlayerHUDWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void SetHealth(float CurrentHealth, float MaxHealth);
	void SetCrosshairSpread(float Spread);
	void SetAiming(bool bIsAiming);
	void SetShowHitMarker(bool bIsKill);
	void SetAmmo(int32 CurrentAmmo, int32 ReserveAmmo, int32 MagazineSize);
	
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	
	//HUD의 심장박동을 위한 변수 값
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> BloodOverlay;
	UPROPERTY(Transient, meta=(BindWidgetAnim))
	TObjectPtr<UWidgetAnimation> HeartbeatAnim;
	UPROPERTY(EditDefaultsOnly, Category="UI")
	float LowHealthThreshold = 0.3f;
	
	//HUD 저체력일때 심장박동을 위한 함수
	void UpdateLowHealthEffect(float HealthPercent);
	
	//HUD 저체력시 핏빛 애니메이션을 위한 텍스처 배열
	UPROPERTY(EditDefaultsOnly, Category="UI")
	TArray<TObjectPtr<UTexture2D>> BloodStageTextures;
	int32 CurrentBloodStage = INDEX_NONE;
	
	//조준선 : 위젯과 간격 설정
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UOverlay> Crosshair;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> CrosshairCenter;
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
	
	//타격 피드백을 위한 값
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> HitMarker;
	UPROPERTY(Transient, meta=(BindWidgetAnim))
	TObjectPtr<UWidgetAnimation> HitMarkerAnim;
	UPROPERTY(EditDefaultsOnly, Category="UI")
	FLinearColor HitColor = FLinearColor(0.85f, 0.81f, 0.75f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category="UI")
	FLinearColor KillColor = FLinearColor(0.7f, 0.13f, 0.23f, 1.f);
	
	//부품 조립을 위한 값
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UAmmoWidget> AmmoWidget;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UHealthBarWidget> HealthBarWidget;
	
};
