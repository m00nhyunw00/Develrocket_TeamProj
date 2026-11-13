#include "AI/Services/Monster/BTService_MonsterLeapReady.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/Monster/TBGameplayAbility_Monster_Leap.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/Monster/TBMonster.h"

UBTService_MonsterLeapReady::UBTService_MonsterLeapReady()
{
	NodeName = TEXT("TB Update Monster Leap Ready");
	Interval = 0.2f;
	RandomDeviation = 0.0f;
	bNotifyTick = true;
	bNotifyCeaseRelevant = true;
	TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(ThisClass, TargetKey), AActor::StaticClass());
	TargetKey.SelectedKeyName = TEXT("TargetActor");
	LeapReadyKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(ThisClass, LeapReadyKey));
	LeapReadyKey.SelectedKeyName = TEXT("CanMonsterLeap");
}

void UBTService_MonsterLeapReady::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);
	if (UBlackboardData* BlackboardAsset = GetBlackboardAsset())
	{
		TargetKey.ResolveSelectedKey(*BlackboardAsset);
		LeapReadyKey.ResolveSelectedKey(*BlackboardAsset);
	}
}

void UBTService_MonsterLeapReady::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard || LeapReadyKey.GetSelectedKeyID() == FBlackboard::InvalidKey) return;
	const AAIController* Controller = OwnerComp.GetAIOwner();
	ATBMonster* Monster = Controller ? Cast<ATBMonster>(Controller->GetPawn()) : nullptr;
	AActor* Target = Cast<AActor>(Blackboard->GetValueAsObject(TargetKey.SelectedKeyName));
	UAbilitySystemComponent* ASC = IsValid(Monster) ? Monster->GetAbilitySystemComponent() : nullptr;
	const TSubclassOf<UTBGameplayAbility_Monster_Leap> LeapClass = IsValid(Monster) ? Monster->GetLeapAbilityClass() : nullptr;
	FGameplayAbilitySpec* Spec = IsValid(ASC) && LeapClass ? ASC->FindAbilitySpecFromClass(LeapClass) : nullptr;
	const UTBGameplayAbility_Monster_Leap* Ability = Spec ? Cast<UTBGameplayAbility_Monster_Leap>(Spec->Ability) : nullptr;
	const bool bReady = Ability && !Spec->IsActive() && ASC->AbilityActorInfo.IsValid()
		&& ASC->GetAvatarActor() == Monster && Ability->CanLeapAtTarget(Monster, Target)
		&& Ability->CanActivateAbility(Spec->Handle, ASC->AbilityActorInfo.Get());
	if (Blackboard->GetValueAsBool(LeapReadyKey.SelectedKeyName) != bReady)
	{
		Blackboard->SetValueAsBool(LeapReadyKey.SelectedKeyName, bReady);
	}
}

void UBTService_MonsterLeapReady::OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent(); Blackboard && LeapReadyKey.GetSelectedKeyID() != FBlackboard::InvalidKey)
	{
		Blackboard->SetValueAsBool(LeapReadyKey.SelectedKeyName, false);
	}
	Super::OnCeaseRelevant(OwnerComp, NodeMemory);
}
