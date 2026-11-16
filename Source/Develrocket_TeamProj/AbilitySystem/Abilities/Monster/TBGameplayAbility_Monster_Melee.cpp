#include "AbilitySystem/Abilities/Monster/TBGameplayAbility_Monster_Melee.h"

#include "TBGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/Monster/TBMonster.h"
#include "Character/Human/TBCitizen.h"
#include "Character/Player/TBPlayer.h"
#include "GameFramework/Character.h"
#include "Interface/TBHitInterface.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/BodySetup.h"
#include "TimerManager.h"

UTBGameplayAbility_Monster_Melee::UTBGameplayAbility_Monster_Melee()
{
	SetAssetTags(FGameplayTagContainer(TBGameplayTags::Ability_Combat));
	ActivationOwnedTags.AddTag(TBGameplayTags::State_Combat);
	ActivationOwnedTags.AddTag(TBGameplayTags::State_MovementBlocked);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Stunned);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_MovementBlocked);
}

void UTBGameplayAbility_Monster_Melee::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AttackTarget.Reset();
	bTargetHit = false;
	if (ATBMonster* Monster = ActorInfo && ActorInfo->IsNetAuthority() ? Cast<ATBMonster>(ActorInfo->AvatarActor.Get()) : nullptr)
	{
		const AAIController* Controller = Cast<AAIController>(Monster->GetController());
		const UBlackboardComponent* Blackboard = Controller ? Controller->GetBlackboardComponent() : nullptr;
		AttackTarget = Blackboard ? Cast<ACharacter>(Blackboard->GetValueAsObject(TEXT("TargetActor"))) : nullptr;
		if (USkeletalMeshComponent* Mesh = Monster->GetMesh())
		{
			AttackMesh = Mesh;
			PreviousTickOption = Mesh->VisibilityBasedAnimTickOption;
			Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		}
	}
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UTBGameplayAbility_Monster_Melee::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	EndHitWindow();
	AttackTarget.Reset();
	bTargetHit = false;
	if (USkeletalMeshComponent* Mesh = AttackMesh.Get()) Mesh->VisibilityBasedAnimTickOption = PreviousTickOption;
	AttackMesh.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UTBGameplayAbility_Monster_Melee::BeginHitWindow()
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	if (!IsActive() || !IsValid(Avatar) || !Avatar->HasAuthority() || !World) return;
	CheckHitWindow();
	if (IsActive() && AttackTarget.IsValid() && !bTargetHit) World->GetTimerManager().SetTimer(HitTimer, this, &ThisClass::CheckHitWindow, 0.1f, true);
}

void UTBGameplayAbility_Monster_Melee::EndHitWindow()
{
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(HitTimer);
}

void UTBGameplayAbility_Monster_Melee::CheckHitWindow()
{
	ATBMonster* Monster = Cast<ATBMonster>(GetAvatarActorFromActorInfo());
	if (!IsActive() || !IsValid(Monster) || !Monster->HasAuthority() || Monster->IsDead()) return;
	USkeletalMeshComponent* Mesh = Monster->GetMesh();
	if (!IsValid(Mesh)) return;
	{
		ACharacter* Target = AttackTarget.Get();
		// 클래스는 공격 대상 종류를, ASC는 Avatar 연결과 사망 상태를 확인한다.
		UAbilitySystemComponent* TargetASC = IsValid(Target) ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target) : nullptr;
		if (!IsValid(TargetASC) || TargetASC->GetAvatarActor() != Target || bTargetHit
			|| TargetASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead)
			|| (!Target->IsA<ATBPlayer>() && !Target->IsA<ATBCitizen>())
			|| !Target->GetClass()->ImplementsInterface(UTBHitInterface::StaticClass())) { EndHitWindow(); return; }
		UCapsuleComponent* Capsule = Target->GetCapsuleComponent();
		if (!IsValid(Capsule) || Capsule->GetCollisionEnabled() == ECollisionEnabled::NoCollision) return;
		const FCollisionShape PlayerShape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
		for (const FName Bone : AttackBodyBones)
		{
			FBodyInstance* Body = Mesh->GetBodyInstance(Bone, false);
			const UBodySetup* Setup = Body ? Body->GetBodySetup() : nullptr;
			if (!Body || !Body->IsValidBodyInstance() || !Setup || Setup->BoneName != Bone) continue;
			if (Body->OverlapTest(Capsule->GetComponentLocation(), Capsule->GetComponentQuat(), PlayerShape))
			{
				if (TargetASC->HasMatchingGameplayTag(TBGameplayTags::State_Invulnerable)) return;
				FHitResult HitResult;
				HitResult.Location = Capsule->GetComponentLocation();
				HitResult.ImpactPoint = HitResult.Location;
				if (ITBHitInterface::Execute_ReceiveAttackHit(Target, FGameplayTag(), AttackDamage, Monster, HitResult))
				{
					bTargetHit = true;
					EndHitWindow();
				}
				break;
			}
		}
	}
}
