#include "AbilitySystem/Abilities/Monster/TBGameplayAbility_Monster_Melee.h"

#include "TBGameplayTags.h"
#include "Character/Monster/TBMonster.h"
#include "Character/Player/TBPlayer.h"
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

	for (TActorIterator<ATBPlayer> It(World); It; ++It)
	{
		ATBPlayer* Player = *It;
		if (!IsValid(Player) || Player->IsDead() || HitActors.Contains(Player)) continue;
		UCapsuleComponent* Capsule = Player->GetCapsuleComponent();
		if (!IsValid(Capsule) || Capsule->GetCollisionEnabled() == ECollisionEnabled::NoCollision) continue;
		const FCollisionShape PlayerShape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
		for (const FName Bone : AttackBodyBones)
		{
			FBodyInstance* Body = Mesh->GetBodyInstance(Bone, false);
			const UBodySetup* Setup = Body ? Body->GetBodySetup() : nullptr;
			if (!Body || !Body->IsValidBodyInstance() || !Setup || Setup->BoneName != Bone) continue;
			if (Body->OverlapTest(Capsule->GetComponentLocation(), Capsule->GetComponentQuat(), PlayerShape))
			{
				if (Player->ApplyDamage(AttackDamage, Monster)) HitActors.Add(Player);
				break;
			}
		}
	}
}
