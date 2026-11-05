#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Network/TBSessionSubsystem.h"
#include "TBSessionListWidget.generated.h"

class UButton;
class UScrollBox;
class UTextBlock;
class UTBSessionRowWidget;

// "뒤로 가기를 눌렀다" 라는 것을 알리는 델리게이트 (매개변수 없음)
DECLARE_MULTICAST_DELEGATE(FOnTBSessionListBack);

UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBSessionListWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	// MainMenu가 이 화면을 열 때 호출
	void StartSearch();
	
	// MainMenu가 구독할 델리게이트
	FOnTBSessionListBack OnBackRequested;
	
protected:
	virtual void NativeOnInitialized() override; // 버튼 바인딩
	virtual void NativeConstruct() override; // Subsystem 구독
	virtual void NativeDestruct() override; // Subsystem 구독 해제
	
	// 에디터에서 WBP_SessionRow를 지정할 칸
	UPROPERTY(EditDefaultsOnly, Category="Session")
	TSubclassOf<UTBSessionRowWidget> RowWidgetClass;
	
	UPROPERTY(EditDefaultsOnly, Category="Session")
	int32 MaxSearchResults = 20;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UScrollBox> SessionScrollBox;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> StatusText;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> RefreshButton;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> BackButton;
	
private:
	UFUNCTION()
	void OnRefreshClicked();
	
	UFUNCTION()
	void OnBackClicked();
	
	// Subsystem 방송을 받는 함수 -> 일반 델리게이트라 UFUCNTION 불필요
	void HandleSessionsFound(const TArray<FTBSessionInfo>& Sessions, bool bWasSuccessful);
	
	UTBSessionSubsystem* GetSessionSubsystem() const;
};