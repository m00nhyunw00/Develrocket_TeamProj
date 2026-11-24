#include "AbilitySystem/GameplayCues/TBGameplayCue_AttackVisual.h"

#include "Character/Player/TBPlayer.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"

bool UTBGameplayCue_AttackVisual::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	// ASC의 Owner가 PlayerState일 수 있으므로 EffectCauser 사용
	ATBPlayer* Player = Cast<ATBPlayer>(Parameters.EffectCauser.Get());

	if (!IsValid(Player) || Player->GetNetMode() == NM_DedicatedServer
		|| !FMath::IsFinite(EffectDuration) || EffectDuration <= 0.0f)
	{
		return false;
	}

	USkeletalMeshComponent* Mesh = Player->IsLocallyControlled()
		? Player->GetFirstPersonMesh() : Player->GetMesh();

	UNiagaraSystem* System = Parameters.NormalizedMagnitude > 0.5f
		? EnhancedSystem.Get() : NormalSystem.Get();

	if (!IsValid(Mesh) || !IsValid(System) || (!SocketName.IsNone() && !Mesh->DoesSocketExist(SocketName)))
	{
		return false;
	}

	UNiagaraComponent* Particle =
		UNiagaraFunctionLibrary::SpawnSystemAttached(
			System, Mesh, SocketName,
			LocationOffset, RotationOffset,
			EAttachLocation::KeepRelativeOffset,
			true, true, ENCPoolMethod::None, false);

	if (!IsValid(Particle))
	{
		return false;
	}

	Particle->SetRelativeScale3D(EffectScale);

	FTimerHandle CleanupTimer;

	Player->GetWorld()->GetTimerManager().SetTimer(
		CleanupTimer,
		FTimerDelegate::CreateWeakLambda(
			Particle,
			[Particle]()
			{
				Particle->DestroyComponent();
			}),
		EffectDuration,
		false);

	return true;
}