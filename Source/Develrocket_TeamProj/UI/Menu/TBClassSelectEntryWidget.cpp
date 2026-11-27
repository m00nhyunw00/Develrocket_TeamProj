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

	if (IconImage && ClassData->Icon)
	{
		IconImage->SetBrushFromTexture(ClassData->Icon.Get());
	}
}

void UTBClassSelectEntryWidget::HandleClicked()
{
	OnEntryClicked.ExecuteIfBound(ClassData);
}
