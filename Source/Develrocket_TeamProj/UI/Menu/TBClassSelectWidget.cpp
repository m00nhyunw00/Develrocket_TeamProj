#include "UI/Menu/TBClassSelectWidget.h"

#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Data/TBCharacterClassData.h"
#include "Framework/TBLobbyManager.h"
#include "Framework/TBPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UI/Menu/TBClassSelectEntryWidget.h"

void UTBClassSelectWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (CloseButton)
	{
		CloseButton->OnClicked.AddDynamic(this, &ThisClass::HandleCloseClicked);
	}

	// 직업 목록은 맵에 배치된 값이라 바뀌지 않음 → 한 번만 생성
	BuildEntries();
}

void UTBClassSelectWidget::BuildEntries()
{
	if (!EntryBox || !EntryWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ClassSelect] EntryBox 또는 EntryWidgetClass 없음"));
		return;
	}

	EntryBox->ClearChildren();

	// 배치 액터라 클라에도 맵 로드 시점부터 존재
	const ATBLobbyManager* Manager = Cast<ATBLobbyManager>(
		UGameplayStatics::GetActorOfClass(this, ATBLobbyManager::StaticClass()));
	if (!Manager)
	{
		return;
	}

	for (const TObjectPtr<UTBCharacterClassData>& Data : Manager->GetAvailableClasses())
	{
		if (!Data)
		{
			continue;
		}

		UTBClassSelectEntryWidget* Entry = CreateWidget<UTBClassSelectEntryWidget>(this, EntryWidgetClass);
		if (!Entry)
		{
			continue;
		}

		Entry->Setup(Data);
		Entry->OnEntryClicked.BindUObject(this, &ThisClass::HandleEntryClicked);
		EntryBox->AddChild(Entry);
	}
}

void UTBClassSelectWidget::OpenSelect()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	SetVisibility(ESlateVisibility::Visible);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(TakeWidget());
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);
}

void UTBClassSelectWidget::CloseSelect()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	SetVisibility(ESlateVisibility::Collapsed);
	PC->SetInputMode(FInputModeGameOnly());
	PC->SetShowMouseCursor(false);
}

void UTBClassSelectWidget::HandleEntryClicked(const UTBCharacterClassData* ClassData)
{
	// 직업 변경 신청 → 서버에서 결재 후 교체
	if (ATBPlayerState* PS = GetOwningPlayerState<ATBPlayerState>())
	{
		PS->RequestClassChange(ClassData);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[ClassSelect] TBPlayerState 없음"));
	}

	CloseSelect();
}

void UTBClassSelectWidget::HandleCloseClicked()
{
	CloseSelect();
}
