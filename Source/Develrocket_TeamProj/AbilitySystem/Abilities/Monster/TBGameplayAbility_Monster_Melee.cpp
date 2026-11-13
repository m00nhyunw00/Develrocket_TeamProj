#include "AbilitySystem/Abilities/Monster/TBGameplayAbility_Monster_Melee.h"

#include "TBGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/Monster/TBMonster.h"
#include "GameFramework/Character.h"
#include "Interface/TBHitInterface.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
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
	HitActors.Reset();
	if (ATBMonster* Monster = ActorInfo && ActorInfo->IsNetAuthority() ? Cast<ATBMonster>(ActorInfo->AvatarActor.Get()) : nullptr)
	{
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
	HitActors.Reset();
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
	if (IsActive()) World->GetTimerManager().SetTimer(HitTimer, this, &ThisClass::CheckHitWindow, 0.02f, true);
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
	UWorld* World = Monster->GetWorld();
	if (!IsValid(Mesh) || !World) return;

	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		ACharacter* Target = *It;
		UAbilitySystemComponent* TargetASC = IsValid(Target) ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target) : nullptr;
		if (!IsValid(TargetASC) || TargetASC->GetAvatarActor() != Target || HitActors.Contains(Target)
			|| TargetASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead)
			|| (!TargetASC->HasMatchingGameplayTag(TBGameplayTags::Character_Type_Player)
				&& !TargetASC->HasMatchingGameplayTag(TBGameplayTags::Character_Type_Human))
			|| !Target->GetClass()->ImplementsInterface(UTBHitInterface::StaticClass())) continue;
		UCapsuleComponent* Capsule = Target->GetCapsuleComponent();
		if (!IsValid(Capsule) || Capsule->GetCollisionEnabled() == ECollisionEnabled::NoCollision) continue;
		const FCollisionShape PlayerShape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
		for (const FName Bone : AttackBodyBones)
		{
			FBodyInstance* Body = Mesh->GetBodyInstance(Bone, false);
			const UBodySetup* Setup = Body ? Body->GetBodySetup() : nullptr;
			if (!Body || !Body->IsValidBodyInstance() || !Setup || Setup->BoneName != Bone) continue;
			if (Body->OverlapTest(Capsule->GetComponentLocation(), Capsule->GetComponentQuat(), PlayerShape))
			{
				FHitResult HitResult;
				HitResult.Location = Capsule->GetComponentLocation();
				HitResult.ImpactPoint = HitResult.Location;
				if (ITBHitInterface::Execute_ReceiveAttackHit(Target, FGameplayTag(), AttackDamage, Monster, HitResult)) HitActors.Add(Target);
				break;
			}
		}
	}
}
