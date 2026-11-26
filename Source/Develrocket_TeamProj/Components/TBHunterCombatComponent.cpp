#include "Components/TBHunterCombatComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/Monster/VampireHunter/TBGameplayAbility_VampireHunter_Sword.h"
#include "AbilitySystem/Attributes/TBMonsterAttributeSet.h"
#include "AI/TBCombatTokenSubsystem.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/Monster/TBVampireHunter.h"
#include "Character/Monster/TBMonster.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "TBGameplayTags.h"


namespace
{
constexpr TCHAR HunterAssets[] = TEXT("/Game/_Assets/Animations/Monster/VampireHunter/");
constexpr TCHAR HunterMeshes[] = TEXT("/Game/_Assets/_Original/Gothic_Hunter_Vampire/Meshes/base_mesh/");
constexpr TCHAR HunterSwordSequences[] = TEXT("/Game/_Assets/_Original/Gothic_Hunter_Vampire/animation/sword/");

FString AssetPath(const TCHAR* Folder, const TCHAR* Name)
{
	return FString::Printf(TEXT("%s%s.%s"), Folder, Name, Name);
}
}

UTBHunterCombatComponent::UTBHunterCombatComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickInterval = 0.08f;
}

void UTBHunterCombatComponent::LoadPhaseAnimations(const UTBHunterAnimationData* RifleData,
	const UTBHunterAnimationData* SwordData)
{
	if (RifleData)
	{
		LoadedRifleMesh = RifleData->Mesh.LoadSynchronous();
		if (UClass* AnimClass = RifleData->AnimClass.LoadSynchronous()) RifleAnimClass = AnimClass;
		RifleAttackMontage = RifleData->RifleAttackMontage.LoadSynchronous();
	}
	else
	{
		LoadedRifleMesh = LoadObject<USkeletalMesh>(nullptr, *AssetPath(HunterMeshes, TEXT("SKM_base_mesh_rifle")));
	}

	if (SwordData)
	{
		LoadedSwordMesh = SwordData->Mesh.LoadSynchronous();
		SwordAnimClass = SwordData->AnimClass.LoadSynchronous();
		SwordFullBodySlot = SwordData->SwordFullBodySlot;
		SwordHitFractions = SwordData->SwordHitFractions;
		for (const auto& [Action, Sequence] : SwordData->SwordActions)
		{
			if (UAnimSequence* Loaded = Sequence.LoadSynchronous()) SwordActions.Add(Action, Loaded);
		}
		return;
	}

	// Existing Hunter Blueprints can still run before assigning their phase data assets.
	LoadedSwordMesh = LoadObject<USkeletalMesh>(nullptr, *AssetPath(HunterMeshes, TEXT("SKM_base_mesh_sword")));
	SwordAnimClass = LoadClass<UAnimInstance>(nullptr, *FString::Printf(TEXT("%sABP_Hunter_Melee.ABP_Hunter_Melee_C"), HunterAssets));
	const auto LoadSwordAction = [this](ETBHunterSwordAnimation Action, const TCHAR* Name)
	{
		SwordActions.Add(Action, LoadObject<UAnimSequence>(nullptr, *AssetPath(HunterSwordSequences, Name)));
	};
	LoadSwordAction(ETBHunterSwordAnimation::Draw, TEXT("anim_sword_rage"));
	LoadSwordAction(ETBHunterSwordAnimation::ApproachSlash, TEXT("anim_sword_attack_3"));
	LoadSwordAction(ETBHunterSwordAnimation::SlashOne, TEXT("anim_sword_attack_1"));
	LoadSwordAction(ETBHunterSwordAnimation::SlashTwo, TEXT("anim_sword_attack_2"));
	LoadSwordAction(ETBHunterSwordAnimation::Hit, TEXT("anim_sword_hit_1"));
	LoadSwordAction(ETBHunterSwordAnimation::Death, TEXT("anim_sword_death"));
}
void UTBHunterCombatComponent::BeginPlay()
{
	Super::BeginPlay();
	const ATBMonster* Hunter = CastChecked<ATBMonster>(GetOwner());
	if (Hunter->GetMonsterBehavior() != ETBMonsterBehavior::Hunter) return;

	static const FName WeaponName(TEXT("Weapon"));
	TInlineComponentArray<USkeletalMeshComponent*> MeshComponents(Hunter);
	for (USkeletalMeshComponent* Component : MeshComponents)
	{
		if (Component->GetFName() == WeaponName)
		{
			Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Component->SetHiddenInGame(true);
			break;
		}
	}
	RifleAnimClass = Hunter->GetMesh()->GetAnimClass();
	const auto* VampireHunter = Cast<ATBVampireHunter>(Hunter);
	LoadPhaseAnimations(VampireHunter ? VampireHunter->GetPhaseAnimationData(false) : nullptr,
		VampireHunter ? VampireHunter->GetPhaseAnimationData(true) : nullptr);
	SetComponentTickEnabled(LoadedSwordMesh && SwordAnimClass);
	OnRep_CombatState();
}

void UTBHunterCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UTBHunterCombatComponent, CombatState);
}

bool UTBHunterCombatComponent::IsAtTransitionHealth() const
{
	const ATBMonster* Hunter = CastChecked<ATBMonster>(GetOwner());
	const auto* Attributes = Hunter->GetAbilitySystemComponent()->GetSet<UTBMonsterAttributeSet>();
	return Attributes && Attributes->GetMaxBlood() > 0.f && Attributes->GetBlood() > 0.f
		&& Attributes->GetBlood() <= Attributes->GetMaxBlood() * SwordPhaseHealthFraction;
}

bool UTBHunterCombatComponent::CanStartRifleAttack() const
{
	return CombatState.Phase == ETBHunterPhase::Rifle
		&& (!LoadedSwordMesh || !SwordAnimClass || (!bTransitionRequested && !IsAtTransitionHealth()));
}

void UTBHunterCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ATBMonster* Hunter = CastChecked<ATBMonster>(GetOwner());
	if (Hunter->HasAuthority() && LoadedSwordMesh && SwordAnimClass && !Hunter->IsDead() && !Hunter->IsHidden())
	{
		if (CombatState.Phase == ETBHunterPhase::Rifle)
		{
			bTransitionRequested |= IsAtTransitionHealth();
			if (bTransitionRequested && !Hunter->IsActionBlocked()
				&& !Hunter->GetAbilitySystemComponent()->HasMatchingGameplayTag(TBGameplayTags::State_Combat))
			{
				if (AController* Controller = Hunter->GetController()) Controller->StopMovement();
				Hunter->GetCharacterMovement()->StopMovementImmediately();
				SetCombatState(ETBHunterPhase::DrawingSword, ETBHunterSwordAnimation::Draw);
			}
		}
		else if (CombatState.Phase == ETBHunterPhase::DrawingSword
			&& GetCombatTime() - CombatState.AnimationStartTime >= FMath::Max(0.8f, GetSwordAnimationDuration(ETBHunterSwordAnimation::Draw)))
		{
			SetCombatState(ETBHunterPhase::Sword, ETBHunterSwordAnimation::Idle);
		}
	}
	if (CombatState.Phase != ETBHunterPhase::Rifle) UpdateSwordVisual();
}

bool UTBHunterCombatComponent::TryStartSwordAttack(AActor& Target)
{
	ATBMonster* Hunter = CastChecked<ATBMonster>(GetOwner());
	if (!Hunter->HasAuthority() || GetPhase() != ETBHunterPhase::Sword
		|| GetWorld()->GetTimeSeconds() < NextSwordAttackTime) return false;

	UAbilitySystemComponent* ASC = Hunter->GetAbilitySystemComponent();
	auto* Tokens = Hunter->GetWorld()->GetSubsystem<UTBCombatTokenSubsystem>();
	const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(UTBGameplayAbility_VampireHunter_Sword::StaticClass());
	if (!Tokens || !ASC->AbilityActorInfo.IsValid() || !Spec || Spec->IsActive() || !Spec->Ability) return false;
	const auto* Sword = CastChecked<UTBGameplayAbility_VampireHunter_Sword>(Spec->Ability);
	const FGameplayAbilitySpecHandle Handle = Spec->Handle;
	if (!Sword->CanStrike(*Hunter, Target) || !Sword->CanActivateAbility(Handle, ASC->AbilityActorInfo.Get())) return false;

	const FTBCombatToken Token = Tokens->TryAcquire(*Hunter, Target, *ASC, Handle);
	if (!Token.Id) return false;

	FGameplayEventData Event;
	Event.EventTag = TBGameplayTags::Ability_Combat_Melee;
	Event.Instigator = Hunter;
	Event.Target = &Target;
	const bool bActivated = ASC->TriggerAbilityFromGameplayEvent(Handle, ASC->AbilityActorInfo.Get(), Event.EventTag, &Event, *ASC);
	Tokens->ReleaseIfInactive(Token);
	const FGameplayAbilitySpec* ActiveSpec = ASC->FindAbilitySpecFromHandle(Handle);
	return bActivated && ActiveSpec && ActiveSpec->IsActive();
}

void UTBHunterCombatComponent::SetSwordAnimation(ETBHunterSwordAnimation Animation)
{
	if (GetOwner()->HasAuthority() && GetPhase() == ETBHunterPhase::Sword) SetCombatState(GetPhase(), Animation);
}

float UTBHunterCombatComponent::GetSwordAnimationDuration(ETBHunterSwordAnimation Animation) const
{
	const auto* Sequence = SwordActions.Find(Animation);
	return Sequence && *Sequence ? (*Sequence)->GetPlayLength() : 0.f;
}

float UTBHunterCombatComponent::GetSwordHitFraction(ETBHunterSwordAnimation Animation) const
{
	const float* Fraction = SwordHitFractions.Find(Animation);
	return Fraction && FMath::IsFinite(*Fraction) ? FMath::Clamp(*Fraction, 0.f, 1.f) : 0.46f;
}

void UTBHunterCombatComponent::DeferSwordAttackRetry()
{
	NextSwordAttackTime = GetWorld()->GetTimeSeconds() + 0.35;
}

double UTBHunterCombatComponent::GetCombatTime() const
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	return GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

void UTBHunterCombatComponent::SetCombatState(ETBHunterPhase Phase, ETBHunterSwordAnimation Animation)
{
	CombatState.Phase = Phase;
	CombatState.Animation = Animation;
	CombatState.AnimationStartTime = GetCombatTime();
	OnRep_CombatState();
	GetOwner()->ForceNetUpdate();
}

void UTBHunterCombatComponent::ResetPhase()
{
	bTransitionRequested = false;
	NextSwordAttackTime = 0.0;
	if (GetPhase() != ETBHunterPhase::Rifle) SetCombatState(ETBHunterPhase::Rifle, ETBHunterSwordAnimation::Idle);
}

void UTBHunterCombatComponent::OnRep_CombatState()
{
	if (!HasBegunPlay()) return;

	const ATBMonster* Hunter = CastChecked<ATBMonster>(GetOwner());
	USkeletalMeshComponent* Mesh = Hunter->GetMesh();
	if (GetPhase() == ETBHunterPhase::Rifle)
	{
		if (LoadedRifleMesh && Mesh->GetSkeletalMeshAsset() != LoadedRifleMesh)
		{
			Mesh->SetSkeletalMesh(LoadedRifleMesh);
		}
		if (RifleAnimClass && Mesh->GetAnimClass() != RifleAnimClass.Get())
		{
			Mesh->SetAnimInstanceClass(RifleAnimClass);
		}
		bSwordAnimationMode = false;
		PlayingAnimation.Reset();
		PlayingSwordMontage.Reset();
		return;
	}
	if (!bSwordAnimationMode && LoadedSwordMesh && SwordAnimClass)
	{
		Mesh->SetSkeletalMesh(LoadedSwordMesh);
		Mesh->SetAnimInstanceClass(SwordAnimClass);
		if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
		{
			AnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
		}
		bSwordAnimationMode = true;
	}
	UpdateSwordVisual();
}

void UTBHunterCombatComponent::UpdateSwordVisual()
{
	if (!bSwordAnimationMode || GetNetMode() == NM_DedicatedServer) return;
	ATBMonster* Hunter = CastChecked<ATBMonster>(GetOwner());
	UAnimInstance* AnimInstance = Hunter->GetMesh()->GetAnimInstance();
	if (!AnimInstance) return;

	ETBHunterSwordAnimation Desired = CombatState.Animation;
	if (Hunter->IsDead()) Desired = ETBHunterSwordAnimation::Death;
	else if (Hunter->GetAbilitySystemComponent()->HasMatchingGameplayTag(TBGameplayTags::State_Stunned)) Desired = ETBHunterSwordAnimation::Hit;

	const bool bTimedAction = Desired == CombatState.Animation;
	if (PlayingAnimation == Desired && (!bTimedAction || PlayingAnimationStartTime == CombatState.AnimationStartTime)) return;
	if (UAnimMontage* Previous = PlayingSwordMontage.Get())
	{
		AnimInstance->Montage_Stop(0.12f, Previous);
		PlayingSwordMontage.Reset();
	}

	const auto* Sequence = SwordActions.Find(Desired);
	if (Sequence && *Sequence)
	{
		const float Length = (*Sequence)->GetPlayLength();
		const double Elapsed = bTimedAction ? GetCombatTime() - CombatState.AnimationStartTime : 0.0;
		if (Elapsed < Length)
		{
			PlayingSwordMontage = AnimInstance->PlaySlotAnimationAsDynamicMontage(*Sequence, SwordFullBodySlot,
				0.10f, 0.12f, 1.f, 1, -1.f, FMath::Max(0.f, static_cast<float>(Elapsed)));
		}
	}
	PlayingAnimation = Desired;
	PlayingAnimationStartTime = CombatState.AnimationStartTime;
}
