#include "UI/HUD/TBSkillBarWidget.h"
#include "UI/HUD/TBSkillSlotWidget.h"

void UTBSkillBarWidget::SetSkill(int32 Index, UTexture2D* Icon, const FText& Key)
{
	UTBSkillSlotWidget* SkillSlot = GetSlot(Index);
	if (!SkillSlot)
	{
		return;
	}
	SkillSlot->SetIcon(Icon);
	SkillSlot->SetKey(Key);
}

void UTBSkillBarWidget::SetSkillCooldownRemaining(int32 Index, float RemainingSeconds)
{
	UTBSkillSlotWidget* SkillSlot = GetSlot(Index);
	if (!SkillSlot)
	{
		return;
	}
	SkillSlot->SetCooldownRemaining(RemainingSeconds);
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
