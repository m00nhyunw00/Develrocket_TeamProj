// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "TBPlayerHUD.generated.h"

class UTBPlayerHUDWidget;
class UTBPauseMenuWidget;

/**
 * 
 */
UCLASS()
class DEVELROCKET_TEAMPROJ_API ATBPlayerHUD : public AHUD
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
	UFUNCTION(Exec)
	void TestSkill(int32 Index, float Duration); // Test Only : 스킬 시스템 연결시 삭제
	
	void TogglePauseMenu();
	UFUNCTION(Exec)
	void TestPause();
	
	
protected:
	virtual void BeginPlay() override;
	
	//HUD 위젯 관련
	UPROPERTY(EditDefaultsOnly, Category="UI")
	TSubclassOf<UTBPlayerHUDWidget> HUDWidgetClass;
	UPROPERTY()
	TObjectPtr<UTBPlayerHUDWidget> HUDWidget;
	
	//PauseMenu관련
	UPROPERTY(EditDefaultsOnly, Category="UI")
	TSubclassOf<UTBPauseMenuWidget> PauseWidgetClass;
	UPROPERTY()
	TObjectPtr<UTBPauseMenuWidget> PauseWidget;
	
	// Test Only : HUD 테스트 용 변수
	float TestHealth = 100.f;
	float TestMaxHealth = 100.f;
	int32 TestAmmo = 30;
	int32 TestReserveAmmo = 90;
	int32 TestMagazineSize = 30;
};
