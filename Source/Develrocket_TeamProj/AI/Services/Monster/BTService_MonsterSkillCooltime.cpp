#include "AI/Services/Monster/BTService_MonsterSkillCooltime.h"

#include "AI/Tasks/Monster/BTTask_MonsterSkill.h"
#include "AI/Tasks/Monster/BTTask_MonsterAttack.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"

UBTService_MonsterSkillCooltime::UBTService_MonsterSkillCooltime()
{
	NodeName = TEXT("TB Monster Skill Cooltime");
	bCreateNodeInstance = true;
	Interval = 0.2f;
	RandomDeviation = 0.0f;
	bNotifyTick = true;
	bNotifyCeaseRelevant = true;
}

void UBTService_MonsterSkillCooltime::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
	UpdateCombatDuringPursuit(OwnerComp);
}

void UBTService_MonsterSkillCooltime::UpdateCombatDuringPursuit(UBehaviorTreeComponent& OwnerComp)
{
	const UBTNode* ActiveNode = OwnerComp.GetActiveNode();
	const UBTComposite_Selector* Selector = Cast<UBTComposite_Selector>(GetParentNode());
	if (!Selector || !Cast<UBTTask_MoveTo>(ActiveNode) || ActiveNode->GetParentNode() != Selector)
	{
		RequestedChildIndex = INDEX_NONE;
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
		const UBTTask_MonsterAttack* AttackTask = Cast<UBTTask_MonsterAttack>(Child);
		if ((SkillTask && SkillTask->HasUsableSkill(OwnerComp)) || (AttackTask && AttackTask->HasUsableAttack(OwnerComp)))
		{
			if (RequestedChildIndex != Index)
			{
				// 같은 분기가 계속 준비된 상태에서는 재탐색 요청을 반복하지 않는다.
				RequestedChildIndex = Index;
				OwnerComp.RequestExecution(Selector, OwnerComp.GetActiveInstanceIdx(), Child, Index, EBTNodeResult::Aborted);
			}
			return;
		}
	}

	RequestedChildIndex = INDEX_NONE;
}

void UBTService_MonsterSkillCooltime::OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	RequestedChildIndex = INDEX_NONE;
	Super::OnCeaseRelevant(OwnerComp, NodeMemory);
}
