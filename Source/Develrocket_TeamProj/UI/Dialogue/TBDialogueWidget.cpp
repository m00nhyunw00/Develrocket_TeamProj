#include "UI/Dialogue/TBDialogueWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

void UTBDialogueWidget::StartDialogue(AActor* InSpeaker, AActor* InListener, const TArray<FText>& InLines)
{
	Speaker = InSpeaker;
	Listener = InListener;
	Lines = InLines;
	CurrentIndex = 0;
	bIsClosing = false;
	
	if (Lines.IsEmpty())
	{
		CloseDialogue();
		return;
	}
	
	if (Anim_Open)
	{
		PlayAnimation(Anim_Open);
	}
	ShowCurrentLine();
	
	// 입력 : 위젯이 키를 먼저 받고, 처리 안 한 키는 게임으로
	if (APlayerController* PC = GetOwningPlayer())
	{
		FInputModeGameAndUI InputModeGameAndUI;
		
		InputModeGameAndUI.SetWidgetToFocus(TakeWidget());
		InputModeGameAndUI.SetHideCursorDuringCapture(true);
		PC->SetInputMode(InputModeGameAndUI);
		PC->SetShowMouseCursor(false);
	}
	
	// 거리 체크 시작
	GetWorld()->GetTimerManager().SetTimer(DistanceTimerHandle, this, &UTBDialogueWidget::CheckDistance, DistanceCheckInterval, true);
}

void UTBDialogueWidget::AdvanceLine()
{
	if (bIsClosing)
	{
		return;
	}
	
	++CurrentIndex;
	
	// 다음 줄이 없으면 담기, 있으면 표시
	if (!Lines.IsValidIndex(CurrentIndex))
	{
		CloseDialogue();
		return;
	}
	
	ShowCurrentLine();
}

void UTBDialogueWidget::CloseDialogue()
{
	// 중복 닫기 방지
	if (bIsClosing)
	{
		return;
	}
	bIsClosing = true;
	
	GetWorld()->GetTimerManager().ClearTimer(DistanceTimerHandle);
	RestoreGameInput();
	
	// 닫기 연출이 있으면 끝난 뒤 제거(HandleCloseAnimFinished), 없으면 즉시 제거
	if (Anim_Close)
	{
		PlayAnimation(Anim_Close);
	}
	else
	{
		RemoveFromParent();
	}
}

void UTBDialogueWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	
	// 키 입력을 받으려면 포커스를 받을 수 있어야 함
	SetIsFocusable(true);
	
	// 닫기 애니메이션이 끝나면 화면에서 제거
	if (Anim_Close)
	{
		FWidgetAnimationDynamicEvent OnCloseFinished;
		OnCloseFinished.BindDynamic(this, &UTBDialogueWidget::HandleCloseAnimFinished);
		BindToAnimationFinished(Anim_Close, OnCloseFinished);
	}
}

void UTBDialogueWidget::NativeDestruct()
{
	// 위젯이 사라진 뒤 타이머가 호출되지 않도록
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DistanceTimerHandle);
	}
	
	Super::NativeDestruct();
}

FReply UTBDialogueWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();

	// 진행 키: 위젯이 삼켜서 상호작용 GA로 안 넘어가게
	if (Key == EKeys::F || Key == EKeys::SpaceBar || Key == EKeys::Enter)
	{
		AdvanceLine();
		return FReply::Handled();
	}

	// 대화 즉시 종료
	if (Key == EKeys::Escape)
	{
		CloseDialogue();
		return FReply::Handled();
	}

	// 나머지(WASD 등)는 게임으로 흘려보냄 → 걸어서 멀어질 수 있음
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
void UTBDialogueWidget::ShowCurrentLine()
{
	if (!DialogueText || !Lines.IsValidIndex(CurrentIndex))
	{
		return;
	}
	
	DialogueText->SetText(Lines[CurrentIndex]);
	
	if (Anim_LineFadeIn)
	{
		PlayAnimation(Anim_LineFadeIn);
	}
}

void UTBDialogueWidget::CheckDistance()
{
	const AActor* SpeakerActor = Speaker.Get();
	const AActor* ListenerActor = Listener.Get();
	
	// 화자나 청자가 사라졌으면 종료
	if (!SpeakerActor || !ListenerActor)
	{
		CloseDialogue();
		return;
	}
	
	// 제곱끼리 비교
	const float DistSq = FVector::DistSquared2D(SpeakerActor->GetActorLocation(), ListenerActor->GetActorLocation());
	
	if (DistSq > FMath::Square(CloseDistance))
	{
		CloseDialogue();
	}
}

void UTBDialogueWidget::RestoreGameInput()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
}

void UTBDialogueWidget::HandleCloseAnimFinished()
{
	// 닫기 애니메이션이 끝난 뒤 화면에서 제거
	RemoveFromParent();
}

