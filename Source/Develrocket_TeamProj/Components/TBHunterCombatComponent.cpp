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
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
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
constexpr float RifleTracerUpdateInterval = 1.f / 60.f;
constexpr double RifleTracerSpeed = 12000.0;
constexpr double RifleTracerMinTravelTime = 0.14;
constexpr ECollisionChannel HunterHitscanTraceChannel = ECC_GameTraceChannel2;
const FName BeamColorParameter(TEXT("Color"));
const FName ChargeBeamMuzzleSocket(TEXT("weapon_r_muzzle"));
const FName MotionTrailOpacity(TEXT("TrailOpacity"));
constexpr double MotionTrailLifetime = 0.24;
constexpr double MotionTrailSampleInterval = 0.055;

void SetRifleBeamSegment(UStaticMeshComponent& Beam, const FVector& Start, const FVector& End, double Radius)
{
	const FVector Segment = End - Start;
	const FBoxSphereBounds Bounds = Beam.GetStaticMesh()->GetBounds();
	const FQuat Rotation = FRotationMatrix::MakeFromZ(Segment).ToQuat();
	const FVector Scale(Radius / Bounds.BoxExtent.X, Radius / Bounds.BoxExtent.Y, Segment.Size() / (2.0 * Bounds.BoxExtent.Z));
	const FVector Center = (Start + End) * 0.5 - Rotation.RotateVector(Bounds.Origin * Scale);
	Beam.SetWorldTransform(FTransform(Rotation, Center, Scale));
}

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
	static const ConstructorHelpers::FObjectFinder<UMaterialInterface> TracerMaterial(
		TEXT("/Game/_Game/Materials/Hunter/M_HunterRifleTracer.M_HunterRifleTracer"));
	ChargeBeamMesh = BeamMesh.Object;
	ChargeBeamMaterial = BeamMaterial.Object;
	RifleTracerMaterial = TracerMaterial.Object;
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
	ClearChargeBeamPoseUpdate();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChargeBeamTimer);
		World->GetTimerManager().ClearTimer(RifleTracerTimer);
		World->GetTimerManager().ClearTimer(MotionTrailTimer);
	}
	for (const FRifleTracer& Tracer : RifleTracers)
	{
		if (UStaticMeshComponent* Mesh = Tracer.Mesh.Get()) Mesh->DestroyComponent();
	}
	RifleTracers.Reset();
	for (const FMotionTrailGhost& Ghost : MotionTrailGhosts)
	{
		if (UPoseableMeshComponent* Mesh = Ghost.Mesh.Get()) Mesh->DestroyComponent();
	}
	MotionTrailGhosts.Reset();
	if (ChargeBeam)
	{
		ChargeBeam->DestroyComponent();
	}
	Super::EndPlay(EndPlayReason);
}

void UTBHunterCombatComponent::MulticastStartMotionTrail_Implementation(float Duration)
{
	if (GetNetMode() == NM_DedicatedServer || Duration <= 0.f)
	{
		return;
	}
	const ATBVampireHunter* Hunter = Cast<ATBVampireHunter>(GetOwner());
	const UTBHunterAnimationData* Data = Hunter ? Hunter->GetPhaseAnimationData(true) : nullptr;
	if (!Data || !Data->SwordMotionTrailMaterial)
	{
		return;
	}

	UWorld* World = GetWorld();
	const double Now = World->GetTimeSeconds();
	MotionTrailUntil = Now + Duration;
	NextMotionTrailSample = Now;
	if (!World->GetTimerManager().IsTimerActive(MotionTrailTimer))
	{
		World->GetTimerManager().SetTimer(MotionTrailTimer, this, &ThisClass::UpdateMotionTrail, 1.f / 60.f, true);
	}
	UpdateMotionTrail();
}

void UTBHunterCombatComponent::MulticastStopMotionTrail_Implementation()
{
	MotionTrailUntil = 0.0;
}

void UTBHunterCombatComponent::UpdateMotionTrail()
{
	UWorld* World = GetWorld();
	const double Now = World->GetTimeSeconds();
	for (int32 Index = MotionTrailGhosts.Num() - 1; Index >= 0; --Index)
	{
		const FMotionTrailGhost& Ghost = MotionTrailGhosts[Index];
		const double Life = (Now - Ghost.SpawnedAt) / MotionTrailLifetime;
		if (Life >= 1.0 || !Ghost.Mesh.IsValid() || !Ghost.Material.IsValid())
		{
			if (UPoseableMeshComponent* Mesh = Ghost.Mesh.Get()) Mesh->DestroyComponent();
			MotionTrailGhosts.RemoveAtSwap(Index);
		}
		else
		{
			Ghost.Material->SetScalarParameterValue(MotionTrailOpacity, static_cast<float>(0.48 * FMath::Square(1.0 - Life)));
		}
	}

	if (Now < MotionTrailUntil && Now >= NextMotionTrailSample)
	{
		SpawnMotionTrailGhost(Now);
		NextMotionTrailSample = Now + MotionTrailSampleInterval;
	}
	if (Now >= MotionTrailUntil && MotionTrailGhosts.IsEmpty())
	{
		World->GetTimerManager().ClearTimer(MotionTrailTimer);
	}
}

void UTBHunterCombatComponent::SpawnMotionTrailGhost(double Now)
{
	ATBVampireHunter* Hunter = Cast<ATBVampireHunter>(GetOwner());
	const UTBHunterAnimationData* Data = Hunter ? Hunter->GetPhaseAnimationData(true) : nullptr;
	USkeletalMeshComponent* SourceMesh = Hunter ? Hunter->GetMesh() : nullptr;
	if (!Data || !Data->SwordMotionTrailMaterial || !SourceMesh || !SourceMesh->GetSkeletalMeshAsset()
		|| Hunter->IsHidden() || Hunter->IsDead())
	{
		return;
	}

	UPoseableMeshComponent* Ghost = NewObject<UPoseableMeshComponent>(Hunter, NAME_None, RF_Transient);
	Ghost->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ghost->SetCanEverAffectNavigation(false);
	Ghost->SetCastShadow(false);
	Ghost->SetSkinnedAssetAndUpdate(SourceMesh->GetSkeletalMeshAsset());
	Ghost->SetWorldTransform(SourceMesh->GetComponentTransform());
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Data->SwordMotionTrailMaterial, Ghost);
	Material->SetScalarParameterValue(MotionTrailOpacity, 0.48f);
	for (int32 Index = 0; Index < Ghost->GetNumMaterials(); ++Index)
	{
		Ghost->SetMaterial(Index, Material);
	}
	Ghost->RegisterComponent();
	Ghost->CopyPoseFromSkeletalComponent(SourceMesh);
	MotionTrailGhosts.Add({Ghost, Material, Now});
}

void UTBHunterCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UTBHunterCombatComponent, CombatState);
	DOREPLIFETIME(UTBHunterCombatComponent, ChargeVisual);
	DOREPLIFETIME(UTBHunterCombatComponent, RifleAimTarget);
	DOREPLIFETIME(UTBHunterCombatComponent, RifleAimLock);
}

bool UTBHunterCombatComponent::TryGetRifleAimLocation(FVector& OutLocation) const
{
	if (RifleAimLock.EndTime > GetCombatTime())
	{
		OutLocation = RifleAimLock.Location;
		return true;
	}
	if (!IsValid(RifleAimTarget)) return false;
	OutLocation = RifleAimTarget->GetActorLocation();
	return true;
}

void UTBHunterCombatComponent::SetRifleAimLock(const FVector& Location, float Duration)
{
	if (!GetOwner()->HasAuthority()) return;
	RifleAimLock.Location = Location;
	RifleAimLock.EndTime = GetCombatTime() + Duration;
	GetOwner()->ForceNetUpdate();
}

void UTBHunterCombatComponent::ClearRifleAimLock()
{
	if (!GetOwner()->HasAuthority() || RifleAimLock.EndTime == 0.0) return;
	RifleAimLock = {};
	GetOwner()->ForceNetUpdate();
}

void UTBHunterCombatComponent::SetRifleAimTarget(AActor* Target)
{
	if (RifleAimTarget == Target) return;
	RifleAimTarget = Target;
	GetOwner()->ForceNetUpdate();
}

bool UTBHunterCombatComponent::TryGetRifleAimRay(const ATBMonster& Hunter, FName MuzzleSocket, const FVector& AimLocation,
	float Range, FVector& OutStart, FVector& OutEnd)
{
	if (MuzzleSocket.IsNone())
	{
		FRotator EyeRotation;
		Hunter.GetActorEyesViewPoint(OutStart, EyeRotation);
	}
	else
	{
		const USkeletalMeshComponent* Mesh = Hunter.GetMesh();
		if (!Mesh || !Mesh->DoesSocketExist(MuzzleSocket)) return false;
		OutStart = Mesh->GetSocketLocation(MuzzleSocket);
	}

	const FVector AimDirection = (AimLocation - OutStart).GetSafeNormal();
	if (AimDirection.IsNearlyZero()) return false;
	OutEnd = OutStart + AimDirection * Range;
	return true;
}

bool UTBHunterCombatComponent::TraceRifleShot(const ATBMonster& Hunter, const FVector& Start, const FVector& End, FHitResult& OutHit)
{
	return Hunter.LineTraceByChannel(OutHit, Start, End, HunterHitscanTraceChannel);
}

void UTBHunterCombatComponent::BeginRifleChargeVisual(AActor* Target, float Duration)
{
	if (!GetOwner()->HasAuthority() || !IsValid(Target))
	{
		return;
	}
	ChargeVisual.Target = Target;
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
		if (USkeletalMeshComponent* Mesh = CastChecked<ATBMonster>(GetOwner())->GetMesh(); Mesh && !ChargeBeamPoseHandle.IsValid())
		{
			ChargeBeamPoseHandle = Mesh->RegisterOnBoneTransformsFinalizedDelegate(
				FOnBoneTransformsFinalizedMultiCast::FDelegate::CreateUObject(this, &ThisClass::UpdateChargeBeam));
		}
	}
	UpdateChargeBeam();
}

void UTBHunterCombatComponent::ClearChargeBeamPoseUpdate()
{
	if (!ChargeBeamPoseHandle.IsValid()) return;
	if (const ATBMonster* Hunter = Cast<ATBMonster>(GetOwner()))
	{
		if (USkeletalMeshComponent* Mesh = Hunter->GetMesh())
		{
			Mesh->UnregisterOnBoneTransformsFinalizedDelegate(ChargeBeamPoseHandle);
		}
	}
	ChargeBeamPoseHandle.Reset();
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
		ClearChargeBeamPoseUpdate();
	}

	const double WarningDuration = FMath::Min(ChargeBeamWarningDuration, static_cast<double>(ChargeVisual.Duration));
	const bool bWarning = bCharging && TimeUntilFire <= WarningDuration;
	const bool bBlinkOff = bWarning && FMath::Frac((1.0 - TimeUntilFire / WarningDuration) * ChargeBeamWarningPulses) >= 0.5;
	const AActor* Target = ChargeVisual.Target.Get();
	const USkeletalMeshComponent* Mesh = Hunter->GetMesh();
	const UAbilitySystemComponent* ASC = Hunter->GetAbilitySystemComponent();
	const bool bInterrupted = Hunter->IsDead() || (ASC && ASC->HasMatchingGameplayTag(TBGameplayTags::State_Stunned));
	if (!bCharging || bBlinkOff || bInterrupted || !IsValid(Target) || Hunter->IsHidden() || GetPhase() != ETBHunterPhase::Rifle
		|| !Mesh || !Mesh->DoesSocketExist(ChargeBeamMuzzleSocket))
	{
		if (ChargeBeam)
		{
			ChargeBeam->SetVisibility(false);
		}
		return;
	}

	const FVector Start = Mesh->GetSocketLocation(ChargeBeamMuzzleSocket);
	const FVector End = Target->GetActorLocation();
	if (FVector::DistSquared(Start, End) <= FMath::Square(UE_KINDA_SMALL_NUMBER) || !ChargeBeamMesh || !ChargeBeamMaterial)
	{
		if (ChargeBeam)
		{
			ChargeBeam->SetVisibility(false);
		}
		return;
	}

	if (!ChargeBeam)
	{
		ChargeBeam = CreateRifleBeam(ChargeBeamMaterial);
		ChargeBeamMaterialInstance = ChargeBeam->CreateDynamicMaterialInstance(0);
	}

	SetRifleBeamSegment(*ChargeBeam, Start, End, 1.5);
	if (ChargeBeamMaterialInstance)
	{
		const float Progress = static_cast<float>(FMath::Clamp(Elapsed / ChargeVisual.Duration, 0.0, 1.0));
		const FLinearColor Color = FLinearColor(1.f, 0.22f, 0.04f) * FMath::Lerp(1.4f, 6.f, Progress);
		ChargeBeamMaterialInstance->SetVectorParameterValue(BeamColorParameter, FLinearColor(Color.R, Color.G, Color.B, 1.f));
	}
	ChargeBeam->SetVisibility(true);
}

UStaticMeshComponent* UTBHunterCombatComponent::CreateRifleBeam(UMaterialInterface* Material)
{
	UStaticMeshComponent* Beam = NewObject<UStaticMeshComponent>(GetOwner());
	Beam->SetMobility(EComponentMobility::Movable);
	Beam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Beam->SetGenerateOverlapEvents(false);
	Beam->SetCanEverAffectNavigation(false);
	Beam->SetCastShadow(false);
	Beam->SetStaticMesh(ChargeBeamMesh);
	Beam->SetMaterial(0, Material);
	Beam->SetVisibility(false);
	Beam->RegisterComponent();
	return Beam;
}

void UTBHunterCombatComponent::MulticastPlayRifleTracer_Implementation(FVector_NetQuantize10 TraceStart,
	FVector_NetQuantize10 Impact, ETBHunterRifleTracer TracerType)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	FTimerManager& Timers = GetWorld()->GetTimerManager();
	const bool bCharged = TracerType == ETBHunterRifleTracer::Charged;
	if (bCharged)
	{
		Timers.ClearTimer(ChargeBeamTimer);
		ClearChargeBeamPoseUpdate();
		if (ChargeBeam) ChargeBeam->SetVisibility(false);
	}
	if (!ChargeBeamMesh || !RifleTracerMaterial) return;

	const FVector Start = TraceStart;
	const FVector Segment = Impact - Start;
	const double Length = Segment.Size();
	if (Length <= UE_KINDA_SMALL_NUMBER) return;

	UStaticMeshComponent* Mesh = CreateRifleBeam(RifleTracerMaterial);
	const bool bBasic = TracerType == ETBHunterRifleTracer::Basic;
	const double Speed = bBasic ? Length / BasicTracerDuration : FMath::Min(RifleTracerSpeed, Length / RifleTracerMinTravelTime);
	const FVector Direction = Segment / Length;
	const double Radius = bCharged ? 2.2 : 1.35;
	const FVector InitialEnd = bBasic ? FVector(Impact) : Start + Direction * FMath::Min(12.0, Length * 0.5);
	SetRifleBeamSegment(*Mesh, Start, InitialEnd, Radius);
	Mesh->SetVisibility(true);
	RifleTracers.Add(FRifleTracer{Mesh, Start, Direction, Length, GetWorld()->GetTimeSeconds(), bBasic ? Length : 0.0, Speed,
		bBasic ? Length : FMath::Min(Length * 0.5, bCharged ? 185.0 : 110.0), Radius});
	if (!Timers.IsTimerActive(RifleTracerTimer))
	{
		Timers.SetTimer(RifleTracerTimer, this, &ThisClass::UpdateRifleTracers, RifleTracerUpdateInterval, true);
	}
}

void UTBHunterCombatComponent::UpdateRifleTracers()
{
	const double Time = GetWorld()->GetTimeSeconds();
	for (int32 Index = RifleTracers.Num() - 1; Index >= 0; --Index)
	{
		const FRifleTracer& Tracer = RifleTracers[Index];
		UStaticMeshComponent* Mesh = Tracer.Mesh.Get();
		const double Distance = Tracer.InitialHeadDistance + (Time - Tracer.StartTime) * Tracer.Speed;
		const double Tail = FMath::Max(0.0, Distance - Tracer.TailLength);
		if (!Mesh || Tail >= Tracer.Length)
		{
			if (Mesh) Mesh->DestroyComponent();
			RifleTracers.RemoveAtSwap(Index);
			continue;
		}
		const double Head = FMath::Min(Distance, Tracer.Length);
		if (Head <= Tail) continue;
		SetRifleBeamSegment(*Mesh, Tracer.Start + Tracer.Direction * Tail, Tracer.Start + Tracer.Direction * Head, Tracer.Radius);
		Mesh->SetVisibility(true);
	}
	if (RifleTracers.IsEmpty())
	{
		GetWorld()->GetTimerManager().ClearTimer(RifleTracerTimer);
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
	ClearRifleAimLock();
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
	if (!bSwordAnimationMode) return;
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

	const bool bSwordSwing = Desired == ETBHunterSwordAnimation::SlashOne
		|| Desired == ETBHunterSwordAnimation::SlashTwo || Desired == ETBHunterSwordAnimation::ApproachSlash;
	const auto* Sequence = (GetNetMode() != NM_DedicatedServer || bSwordSwing) ? SwordActions.Find(Desired) : nullptr;
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
