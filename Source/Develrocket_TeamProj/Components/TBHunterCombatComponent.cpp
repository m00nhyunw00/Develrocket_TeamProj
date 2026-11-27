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
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TBGameplayTags.h"
#include "UObject/ConstructorHelpers.h"


namespace
{
constexpr TCHAR HunterAssets[] = TEXT("/Game/_Assets/Animations/Monster/VampireHunter/");
constexpr TCHAR HunterMeshes[] = TEXT("/Game/_Assets/_Original/Gothic_Hunter_Vampire/Meshes/base_mesh/");
constexpr TCHAR HunterSwordSequences[] = TEXT("/Game/_Assets/_Original/Gothic_Hunter_Vampire/animation/sword/");
constexpr float ChargeBeamUpdateInterval = 1.f / 60.f;
constexpr double ChargeBeamWarningDuration = 0.48;
constexpr double ChargeBeamWarningPulses = 3.0;
constexpr float ShotFlashLifetime = 0.14f;
constexpr ECollisionChannel HunterHitscanTraceChannel = ECC_GameTraceChannel2;
const FName BeamColorParameter(TEXT("Color"));
const FName HunterRifleMuzzleSocket(TEXT("weapon_r_muzzle"));

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

	static const ConstructorHelpers::FObjectFinder<UStaticMesh> BeamMesh(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static const ConstructorHelpers::FObjectFinder<UMaterialInterface> BeamMaterial(
		TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"));
	static const ConstructorHelpers::FObjectFinder<UNiagaraSystem> FlashSystem(
		TEXT("/Game/_Assets/Particles/NS_BW_PrimaryAttack_Beam.NS_BW_PrimaryAttack_Beam"));
	ChargeBeamMesh = BeamMesh.Object;
	ChargeBeamMaterial = BeamMaterial.Object;
	ChargedShotFlashSystem = FlashSystem.Object;
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
	OnRep_ChargeVisual();
}

void UTBHunterCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChargeBeamTimer);
	}
	if (ChargeBeam)
	{
		ChargeBeam->DestroyComponent();
	}
	Super::EndPlay(EndPlayReason);
}

void UTBHunterCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UTBHunterCombatComponent, CombatState);
	DOREPLIFETIME(UTBHunterCombatComponent, ChargeVisual);
}

bool UTBHunterCombatComponent::TryGetRifleShotOrigin(const ATBMonster& Hunter, FName MuzzleSocket, FVector& OutOrigin)
{
	if (MuzzleSocket.IsNone())
	{
		FRotator EyeRotation;
		Hunter.GetActorEyesViewPoint(OutOrigin, EyeRotation);
		return true;
	}

	const USkeletalMeshComponent* Mesh = Hunter.GetMesh();
	if (!Mesh || !Mesh->DoesSocketExist(MuzzleSocket))
	{
		return false;
	}
	OutOrigin = Mesh->GetSocketLocation(MuzzleSocket);
	return true;
}

bool UTBHunterCombatComponent::TraceRifleShot(const ATBMonster& Hunter, const FVector& Start, const FVector& End, FHitResult& OutHit)
{
	return Hunter.LineTraceByChannel(OutHit, Start, End, HunterHitscanTraceChannel);
}

void UTBHunterCombatComponent::BeginRifleChargeVisual(AActor* Target, FName MuzzleSocket, float Range, float Duration)
{
	if (!GetOwner()->HasAuthority() || !IsValid(Target))
	{
		return;
	}
	ChargeVisual.Target = Target;
	ChargeVisual.MuzzleSocket = MuzzleSocket.IsNone() ? HunterRifleMuzzleSocket : MuzzleSocket;
	ChargeVisual.Range = Range;
	ChargeVisual.Duration = Duration;
	ChargeVisual.StartTime = GetCombatTime();
	OnRep_ChargeVisual();
	GetOwner()->ForceNetUpdate();
}

void UTBHunterCombatComponent::EndRifleChargeVisual()
{
	if (!GetOwner()->HasAuthority() || ChargeVisual.Duration <= 0.f)
	{
		return;
	}
	ChargeVisual = {};
	OnRep_ChargeVisual();
	GetOwner()->ForceNetUpdate();
}

void UTBHunterCombatComponent::OnRep_ChargeVisual()
{
	if (!HasBegunPlay() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	FTimerManager& Timers = GetWorld()->GetTimerManager();
	Timers.ClearTimer(ChargeBeamTimer);
	if (ChargeVisual.Duration > 0.f)
	{
		Timers.SetTimer(ChargeBeamTimer, this, &ThisClass::UpdateChargeBeam, ChargeBeamUpdateInterval, true);
	}
	UpdateChargeBeam();
}

void UTBHunterCombatComponent::UpdateChargeBeam()
{
	const ATBMonster* Hunter = CastChecked<ATBMonster>(GetOwner());
	const double Elapsed = GetCombatTime() - ChargeVisual.StartTime;
	const double TimeUntilFire = ChargeVisual.Duration - Elapsed;
	const bool bCharging = ChargeVisual.Duration > 0.f && TimeUntilFire > 0.0;
	if (!bCharging)
	{
		GetWorld()->GetTimerManager().ClearTimer(ChargeBeamTimer);
	}

	const double WarningDuration = FMath::Min(ChargeBeamWarningDuration, static_cast<double>(ChargeVisual.Duration));
	const bool bWarning = bCharging && TimeUntilFire <= WarningDuration;
	const bool bBlinkOff = bWarning && FMath::Frac((1.0 - TimeUntilFire / WarningDuration) * ChargeBeamWarningPulses) >= 0.5;
	FVector Start;
	const AActor* Target = ChargeVisual.Target.Get();
	if (!bCharging || bBlinkOff || !IsValid(Target) || Hunter->IsHidden() || GetPhase() != ETBHunterPhase::Rifle
		|| !TryGetRifleShotOrigin(*Hunter, ChargeVisual.MuzzleSocket, Start))
	{
		if (ChargeBeam)
		{
			ChargeBeam->SetVisibility(false);
		}
		return;
	}

	FVector End = Start + (Target->GetActorLocation() - Start).GetClampedToMaxSize(ChargeVisual.Range);
	FHitResult Hit;
	if (TraceRifleShot(*Hunter, Start, End, Hit))
	{
		End = Hit.ImpactPoint;
	}
	const FVector Segment = End - Start;
	const double Length = Segment.Size();
	if (Length <= UE_KINDA_SMALL_NUMBER || !ChargeBeamMesh || !ChargeBeamMaterial)
	{
		if (ChargeBeam)
		{
			ChargeBeam->SetVisibility(false);
		}
		return;
	}

	if (!ChargeBeam)
	{
		ChargeBeam = NewObject<UStaticMeshComponent>(GetOwner());
		ChargeBeam->SetMobility(EComponentMobility::Movable);
		ChargeBeam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ChargeBeam->SetGenerateOverlapEvents(false);
		ChargeBeam->SetCanEverAffectNavigation(false);
		ChargeBeam->SetCastShadow(false);
		ChargeBeam->SetStaticMesh(ChargeBeamMesh);
		ChargeBeam->SetMaterial(0, ChargeBeamMaterial);
		ChargeBeamMaterialInstance = ChargeBeam->CreateDynamicMaterialInstance(0);
		ChargeBeam->SetVisibility(false);
		ChargeBeam->RegisterComponent();
	}

	const FBoxSphereBounds Bounds = ChargeBeamMesh->GetBounds();
	const FQuat Rotation = FRotationMatrix::MakeFromZ(Segment).ToQuat();
	const FVector Scale(1.5 / Bounds.BoxExtent.X, 1.5 / Bounds.BoxExtent.Y, Length / (2.0 * Bounds.BoxExtent.Z));
	const FVector Center = Start + Segment * 0.5 - Rotation.RotateVector(Bounds.Origin * Scale);
	ChargeBeam->SetWorldTransform(FTransform(Rotation, Center, Scale));
	if (ChargeBeamMaterialInstance)
	{
		const float Progress = static_cast<float>(FMath::Clamp(Elapsed / ChargeVisual.Duration, 0.0, 1.0));
		const FLinearColor Color = FLinearColor(1.f, 0.22f, 0.04f) * FMath::Lerp(1.4f, 6.f, Progress);
		ChargeBeamMaterialInstance->SetVectorParameterValue(BeamColorParameter, FLinearColor(Color.R, Color.G, Color.B, 1.f));
	}
	ChargeBeam->SetVisibility(true);
}

void UTBHunterCombatComponent::MulticastPlayChargedShotFlash_Implementation(FVector_NetQuantize10 Start, FVector_NetQuantize10 End)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	GetWorld()->GetTimerManager().ClearTimer(ChargeBeamTimer);
	if (ChargeBeam)
	{
		ChargeBeam->SetVisibility(false);
	}
	if (!ChargedShotFlashSystem || FVector::DistSquared(Start, End) <= UE_KINDA_SMALL_NUMBER)
	{
		return;
	}
	UNiagaraComponent* Flash = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetWorld(), ChargedShotFlashSystem, Start, (End - Start).Rotation(), FVector::OneVector,
		true, false, ENCPoolMethod::None, false);
	if (Flash)
	{
		Flash->SetVariablePosition(TEXT("User.TrailStart"), Start);
		Flash->SetVariablePosition(TEXT("User.TrailEnd"), End);
		Flash->Activate(true);

		FTimerHandle FlashTimer;
		GetWorld()->GetTimerManager().SetTimer(FlashTimer, FTimerDelegate::CreateWeakLambda(Flash, [Flash]()
		{
			Flash->DestroyComponent();
		}), ShotFlashLifetime, false);
	}
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
	EndRifleChargeVisual();
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
