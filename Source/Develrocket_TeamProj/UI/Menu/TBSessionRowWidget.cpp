#include "UI/Menu/TBSessionRowWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Network/TBSessionSubsystem.h"

void UTBSessionRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	
	if (RowButton)
	{
		// 버튼이 클릭되면 이 객체 (this)의 OnRowButtonClicked를 호출하도록 연결
		// OnClicked는 Dynamic 델리게이트 -> AddDynamic, 대상 함수는 UFUNCTION 필수
		RowButton->OnClicked.AddDynamic(this, &UTBSessionRowWidget::OnRowButtonClicked);
	}
}

void UTBSessionRowWidget::Setup(const FTBSessionInfo& InInfo)
{
	// 클릭 했을떄 subsystem에 넘길 "몇 번째 방"인지 기억
	SearchIndex = InInfo.SearchIndex;
	
	if (HostNameText)
	{
		//FString->FText변환 (TextBlock은 FText만 받음)
		HostNameText->SetText(FText::FromString(InInfo.HostName));
	}
	
	if (PlayerCountText)
	{
		// 숫자 2개를 " 3 / 6 " 현태의 FString으로 만든 뒤 FText로 변환
		PlayerCountText->SetText(FText::FromString(
			FString::Printf(TEXT("%d / %d"), InInfo.CurrentPlayers, InInfo.MaxPlayers)));
	}
	
	if (PingText)
	{
		PingText->SetText(FText::FromString(
			FString::Printf(TEXT("%d ms"), InInfo.PingInMs)));
	}
}

void UTBSessionRowWidget::OnRowButtonClicked()
{
	// 위젯 -> GameInstance -> Subsystem 순서로 찾아감
	UGameInstance* GI = GetGameInstance();
	UTBSessionSubsystem* Session = GI ? GI->GetSubsystem<UTBSessionSubsystem>() : nullptr;
	
	if (!Session)
	{
		return;
	}
	
	// 접속 중에 같은 줄을 연타해서 Join 요청이 여러번 가는 것을 막음
	RowButton->SetIsEnabled(false);
	
	// 실제 검색 결과는 Subsystem이 들고 있으니, 번호만 넘김
	Session->JoinSessionByIndex(SearchIndex);
}
