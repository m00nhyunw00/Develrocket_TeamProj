#include "UI/Menu/TBClassSelectEntryWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Data/TBCharacterClassData.h"

void UTBClassSelectEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (SelectButton)
	{
		SelectButton->OnClicked.AddDynamic(this, &ThisClass::HandleClicked);
	}
}

void UTBClassSelectEntryWidget::Setup(const UTBCharacterClassData* InClassData)
{
	ClassData = InClassData;
	if (!ClassData)
	{
		return;
	}

	if (NameText)
	{
		NameText->SetText(ClassData->DisplayName);
	}

	if (PortraitImage && ClassData->Portrait)
	{
		PortraitImage->SetBrushFromTexture(ClassData->Portrait.Get());
	}
}

void UTBClassSelectEntryWidget::SetSelectable(bool bSelectable)
{
	if (SelectButton)
	{
		SelectButton->SetIsEnabled(bSelectable);
	}
}

void UTBClassSelectEntryWidget::HandleClicked()
{
	OnEntryClicked.ExecuteIfBound(ClassData);
}
