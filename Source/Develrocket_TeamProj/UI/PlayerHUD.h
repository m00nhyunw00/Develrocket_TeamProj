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

public:
	UFUNCTION(Exec) 
	void TestDamage(float Amount); //Test Only : 체력 탄약 시스템 연결시 삭제
	UFUNCTION(Exec) 
	void TestHeal(float Amount); //Test Only : 체력 탄약 시스템 연결시 삭제
	UFUNCTION(Exec)
	void TestSpread(float Spread); //Test Only : 탄 퍼짐 시스템 연결시 삭제
	UFUNCTION(Exec)
	void TestAim(bool bIsAiming); // Test Only : 조준 시스템 연결시 삭제
	UFUNCTION(Exec)
	void TestHit(bool bIsKill); // Test Only : 타격 시스템 연결시 삭제
	UFUNCTION(Exec)
	void TestSetAmmo(int32 Current, int32 Reserve); // Test Only : 재장전 시스템 연결시 삭제
	
protected:
	virtual void BeginPlay() override;
	
	UPROPERTY(EditDefaultsOnly, Category="UI")
	TSubclassOf<UPlayerHUDWidget> HUDWidgetClass;
	
	UPROPERTY()
	TObjectPtr<UPlayerHUDWidget> HUDWidget;
	
	// Test Only : HUD 테스트 용 변수
	float TestHealth = 100.f;
	float TestMaxHealth = 100.f;
	int32 TestAmmo = 30;
	int32 TestReserveAmmo = 90;
	int32 TestMagazineSize = 30;
};
