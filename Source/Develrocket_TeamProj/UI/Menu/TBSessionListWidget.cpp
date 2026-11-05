#include "UI/Menu/TBSessionListWidget.h"

#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "UI/Menu/TBSessionRowWidget.h"
#include "UI/Menu/TBSessionListWidget.h"

void UTBSessionListWidget::StartSearch()
{
	UTBSessionSubsystem* Session = GetSessionSubsystem();
	if (!Session)
	{
		return;
	}
	
	SessionScrollBox->ClearChildren();
	
	StatusText->SetText(FText::FromString(TEXT("SEARCHING...")));
	StatusText->SetVisibility(ESlateVisibility::HitTestInvisible);
	
	RefreshButton->SetIsEnabled(false);
	
	Session->FindSessions(MaxSearchResults);
}

void UTBSessionListWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	
	if (RefreshButton)
	{
		RefreshButton->OnClicked.AddDynamic(this, &UTBSessionListWidget::OnRefreshClicked);
	}
	if (BackButton)
	{
		BackButton->OnClicked.AddDynamic(this, &UTBSessionListWidget::OnBackClicked);
	}
}

void UTBSessionListWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (UTBSessionSubsystem* Session = GetSessionSubsystem())
	{
		// 일반(non-dynamic) 델리게이트 구독
		Session->OnSessionsFound.AddUObject(this, &UTBSessionListWidget::HandleSessionsFound);
	}
}

void UTBSessionListWidget::NativeDestruct()
{
	if (UTBSessionSubsystem* Session = GetSessionSubsystem())
	{
		// 이 객체가 걸어둔 구독을 전부 해제
		Session->OnSessionsFound.RemoveAll(this);
	}
	
	Super::NativeDestruct();
}

void UTBSessionListWidget::OnRefreshClicked()
{
	StartSearch();
}

void UTBSessionListWidget::OnBackClicked()
{
	OnBackRequested.Broadcast();
}

void UTBSessionListWidget::HandleSessionsFound(const TArray<FTBSessionInfo>& Sessions, bool bWasSuccessful)
{
	RefreshButton->SetIsEnabled(true);
	SessionScrollBox->ClearChildren();
	
	if (!bWasSuccessful)
	{
		StatusText->SetText(FText::FromString(TEXT("SEARCH FAILED")));
		return;
	}
	
	if (Sessions.Num()==0)
	{
		StatusText->SetText(FText::FromString(TEXT("NO SESSIONS FOUND")));
		return;
	}
	
	// 결과가 있으면 상태 텍스트는 숨기고 그 공간을 목록에 줌
	StatusText->SetVisibility(ESlateVisibility::Collapsed);
	
	if (!RowWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TBSessionList] RowWidgetClass not set"));
		return;
	}
	
	for (const FTBSessionInfo& Info : Sessions)
	{
		// RowWidgetClass(= WBP_SessionRow)로 실제 위젯 인스턴스 생성. 소유자는 this
		UTBSessionRowWidget* Row = CreateWidget<UTBSessionRowWidget>(this, RowWidgetClass);
		if (!Row)
		{
			continue;
		}

		Row->Setup(Info);                 // 텍스트 채우고 SearchIndex 저장
		SessionScrollBox->AddChild(Row);  // 목록에 한 줄 추가
	}
}

UTBSessionSubsystem* UTBSessionListWidget::GetSessionSubsystem() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UTBSessionSubsystem>() : nullptr;
}