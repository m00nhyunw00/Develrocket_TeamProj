// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TBDialogueWidget.generated.h"

class UTextBlock;
class UWidgetAnimation;

/**
 * 자막 대화 위젯 (로컬 전용)
 * - 화자(Speaker)와 청자(Listener) 거리가 CloseDistance를 넘으면 스스로 닫힘
 * - 대화 중 진행 키는 위젯이 먼저 받아서 게임(상호작용 GA)으로 넘어가지 않게함
 */
UCLASS(Abstract)
class DEVELROCKET_TEAMPROJ_API UTBDialogueWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	// 대화 시작: 화자, 청자, 대사 목록
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void StartDialogue(AActor* InSpeaker, AActor* InListener, const TArray<FText>& InLines);
	
	// 다음 줄 (마지막이면 닫기)
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void AdvanceLine();
	
	// 닫기 연출 후 제거 + 입력 복구
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void CloseDialogue();
	
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	
	// WBP 연결 -------------------------
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DialogueText;
	
	UPROPERTY(Transient, meta = (BindWidgetAnim))
	TObjectPtr<UWidgetAnimation> Anim_Open;
	
	UPROPERTY(Transient, meta = (BindWidgetAnim))
	TObjectPtr<UWidgetAnimation> Anim_LineFadeIn;
	
	UPROPERTY(Transient, meta = (BindWidgetAnim))
	TObjectPtr<UWidgetAnimation> Anim_Close;
	
	// 설정 -----------------------------
	
	// 이 거리 이상 멀어지면 대화 종료 
	UPROPERTY(EditDefaultsOnly, Category = "Dialogue", meta = (ClampMin = "0.0", Units = "cm"))
	float CloseDistance = 450.0f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Dialogue", meta = (ClampMin = "0.05", Units = "s"))
	float DistanceCheckInterval = 0.2f;
	
private:
	void ShowCurrentLine();
	void CheckDistance();
	void RestoreGameInput();
	
	// 동적 델리게이트에 바인딩
	UFUNCTION()
	void HandleCloseAnimFinished();
	
	TWeakObjectPtr<AActor> Speaker;
	TWeakObjectPtr<AActor> Listener;
	
	TArray<FText> Lines;
	int32 CurrentIndex = INDEX_NONE;
	
	// 거리 초과와 마지막 줄 넘김이 겹쳐도 한번만 닫히게
	bool bIsClosing = false;
	
	FTimerHandle DistanceTimerHandle;
	
};
