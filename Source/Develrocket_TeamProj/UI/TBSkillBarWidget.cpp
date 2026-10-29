#include "UI/TBSkillBarWidget.h"
#include "UI/TBSkillSlotWidget.h"

void UTBSkillBarWidget::SetSkill(int32 Index, UTexture2D* Icon, const FText& Key)
{
	UTBSkillSlotWidget* SkillSlot = GetSlot(Index);
	if (!Slot)
	{
		return;
	}
	SkillSlot->SetIcon(Icon);
	SkillSlot->SetKey(Key);
}

void UTBSkillBarWidget::StartSkillCooldown(int32 Index, float Duration)
{
	UTBSkillSlotWidget* SkillSlot = GetSlot(Index);
	if (!Slot)
	{
		return;
	}
	SkillSlot->StartCooldown(Duration);
}

UTBSkillSlotWidget* UTBSkillBarWidget::GetSlot(int32 Index) const
{
	if (Index == 0)
	{
		return SkillSlot1;
	}
	if (Index == 1)
	{
		return SkillSlot2;
	}
	return nullptr;
}
