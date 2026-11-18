#include "AI/Services/Monster/BTService_MonsterLeapReady.h"

#include "AI/Tasks/Monster/BTTask_MonsterSkill.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"

UBTService_MonsterLeapReady::UBTService_MonsterLeapReady()
{
	NodeName = TEXT("TB Update Monster Skill Ready");
	bCreateNodeInstance = true;
	Interval = 0.2f;
	RandomDeviation = 0.0f;
	bNotifyTick = true;
	bNotifyCeaseRelevant = true;
}

void UBTService_MonsterLeapReady::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
	UpdateSkillDuringPursuit(OwnerComp);
}

void UBTService_MonsterLeapReady::UpdateSkillDuringPursuit(UBehaviorTreeComponent& OwnerComp)
{
	const UBTNode* ActiveNode = OwnerComp.GetActiveNode();
	const UBTComposite_Selector* Selector = Cast<UBTComposite_Selector>(GetParentNode());
	if (!Selector || !Cast<UBTTask_MoveTo>(ActiveNode) || ActiveNode->GetParentNode() != Selector)
	{
		bRequestedSkillWhileReady = false;
		return;
	}

	for (int32 Index = 0; Index < Selector->GetChildrenNum(); ++Index)
	{
		const UBTNode* Child = Selector->GetChildNode(Index);
		if (Child == ActiveNode || (Child && Child->GetExecutionIndex() >= ActiveNode->GetExecutionIndex()))
		{
			break;
		}

		const UBTTask_MonsterSkill* SkillTask = Cast<UBTTask_MonsterSkill>(Child);
		if (SkillTask && SkillTask->HasUsableSkill(OwnerComp))
		{
			if (!bRequestedSkillWhileReady)
			{
				// Limit failed reactivation attempts while the same skill conditions stay true.
				bRequestedSkillWhileReady = true;
				OwnerComp.RequestExecution(Selector, OwnerComp.GetActiveInstanceIdx(), SkillTask, Index, EBTNodeResult::Aborted);
			}
			return;
		}
	}

	bRequestedSkillWhileReady = false;
}

void UBTService_MonsterLeapReady::OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	bRequestedSkillWhileReady = false;
	Super::OnCeaseRelevant(OwnerComp, NodeMemory);
}
