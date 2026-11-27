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

void UTBClassSelectWidget::NativeDestruct()
{
	if (ATBPlayerState* PS = BoundPlayerState.Get())
	{
		PS->OnClassChangeResult.Remove(ClassChangeResultHandle);
	}

	Super::NativeDestruct();
}

void UTBClassSelectWidget::BuildEntries()
{
	if (!EntryBox || !EntryWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ClassSelect] EntryBox 또는 EntryWidgetClass 없음"));
		return;
	}

	EntryBox->ClearChildren();
	EntryWidgets.Reset();

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
		EntryWidgets.Add(Entry);
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
	BindClassChangeResult();
	RefreshEntries();

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

void UTBClassSelectWidget::RefreshEntries()
{
	// 입장 직후 클라에는 PlayerState가 아직 없을 수 있음 → 미선택으로 처리
	const ATBPlayerState* PS = GetOwningPlayerState<ATBPlayerState>();
	const UTBCharacterClassData* Selected = PS ? PS->GetSelectedClassData() : nullptr;

	for (UTBClassSelectEntryWidget* Entry : EntryWidgets)
	{
		if (Entry)
		{
			Entry->SetSelectable(Entry->GetClassData() != Selected);
		}
	}

	// 아직 직업을 안 골랐으면 닫을 수 없음 (첫 입장)
	if (CloseButton)
	{
		CloseButton->SetVisibility(Selected ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UTBClassSelectWidget::SetAllEntriesSelectable(bool bSelectable)
{
	for (UTBClassSelectEntryWidget* Entry : EntryWidgets)
	{
		if (Entry)
		{
			Entry->SetSelectable(bSelectable);
		}
	}
}

void UTBClassSelectWidget::BindClassChangeResult()
{
	ATBPlayerState* PS = GetOwningPlayerState<ATBPlayerState>();
	if (!PS || BoundPlayerState.Get() == PS)
	{
		return;
	}

	ClassChangeResultHandle = PS->OnClassChangeResult.AddUObject(this, &ThisClass::HandleClassChangeResult);
	BoundPlayerState = PS;
}

void UTBClassSelectWidget::HandleEntryClicked(const UTBCharacterClassData* ClassData)
{
	ATBPlayerState* PS = GetOwningPlayerState<ATBPlayerState>();
	if (!PS)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ClassSelect] TBPlayerState 없음"));
		return;
	}

	BindClassChangeResult();

	// 결과가 올 때까지 연타 방지
	SetAllEntriesSelectable(false);

	// 직업 변경 신청 → 서버 결재 → 결과는 HandleClassChangeResult로 옴
	PS->RequestClassChange(ClassData);
}

void UTBClassSelectWidget::HandleClassChangeResult(ETBClassChangeResult Result)
{
	if (Result == ETBClassChangeResult::Approved || Result == ETBClassChangeResult::NoChange)
	{
		CloseSelect();
		return;
	}

	// 거절: 열린 채로 다시 고를 수 있게
	UE_LOG(LogTemp, Log, TEXT("[ClassSelect] 거절됨 (%d)"), static_cast<int32>(Result));
	RefreshEntries();
}

void UTBClassSelectWidget::HandleCloseClicked()
{
	CloseSelect();
}
