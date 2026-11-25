#include "AbilitySystem/GameplayCues/TBGameplayCue_AttackVisual.h"

#include "Character/Player/TBPlayer.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceSkeletalMesh.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogTBAttackVisual, Log, All);

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

	const FName AttachSocketName = SocketName.IsNone() ? TrailEndSocketName : SocketName;
	const bool bUseSocketTrail = !SocketName.IsNone() && !TrailEndSocketName.IsNone();

	if (!IsValid(Mesh) || !IsValid(System)
		|| (!AttachSocketName.IsNone() && !Mesh->DoesSocketExist(AttachSocketName)))
	{
		return false;
	}

	// Skeletal Mesh DI의 Filtered Sockets는 실제 소켓을 사용
	if (bUseSocketTrail && (TrailMeshParameterName.IsNone()
		|| !Mesh->GetSocketByName(SocketName) || !Mesh->GetSocketByName(TrailEndSocketName)))
	{
		UE_LOG(LogTBAttackVisual, Warning,
			TEXT("%s: Socket trail requires two sockets on %s and a Niagara Skeletal Mesh user parameter."),
			*GetName(), *GetNameSafe(Mesh));
		return false;
	}

	UNiagaraComponent* Particle =
		UNiagaraFunctionLibrary::SpawnSystemAttached(
			System, Mesh, AttachSocketName,
			LocationOffset, RotationOffset,
			EAttachLocation::KeepRelativeOffset,
			true, false, ENCPoolMethod::None, false);

	if (!IsValid(Particle))
	{
		return false;
	}

	Particle->SetRelativeScale3D(EffectScale);

	if (bUseSocketTrail)
	{
		const FString ParameterName = TrailMeshParameterName.ToString();
		UNiagaraDataInterfaceSkeletalMesh* TrailMeshInterface =
			UNiagaraFunctionLibrary::GetSkeletalMeshDataInterface(Particle, ParameterName);
		if (!TrailMeshInterface)
		{
			UE_LOG(LogTBAttackVisual, Warning,
				TEXT("%s: Niagara system %s requires a Skeletal Mesh user parameter named %s for socket trails."),
				*GetName(), *GetNameSafe(System), *ParameterName);
			Particle->DestroyComponent();
			return false;
		}

		// 실행별 DI에서 현재 시점의 메시와 도신 양끝 지정
		TrailMeshInterface->SourceMode = ENDISkeletalMesh_SourceMode::Source;
		TrailMeshInterface->MeshUserParameter.Parameter = FNiagaraVariable();
		TrailMeshInterface->bRequireCurrentFrameData = true;
		TrailMeshInterface->SetSourceComponentFromBlueprints(Mesh);
		TrailMeshInterface->SetFilteredSocketsFromBlueprints({ SocketName, TrailEndSocketName });
	}

	// 첫 프레임부터 소켓 정보와 스케일을 반영하여 재생
	Particle->Activate(true);

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
