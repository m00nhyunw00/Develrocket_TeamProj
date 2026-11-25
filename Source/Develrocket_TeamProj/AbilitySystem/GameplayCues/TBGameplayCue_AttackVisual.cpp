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

	if (!IsValid(Player) || Player->GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	UWorld* World = Player->GetWorld();
	const bool bFirstPerson = Player->IsLocallyControlled();
	USkeletalMeshComponent* Mesh = bFirstPerson ? Player->GetFirstPersonMesh() : Player->GetMesh();

	if (!World || !IsValid(Mesh))
	{
		return false;
	}

	// 자식 Cue가 선택한 설정을 공통 재생 경로에 전달
	FTBAttackVisualSettings Settings;
	if (!ResolveSettings(Parameters, Settings))
	{
		UE_LOG(LogTBAttackVisual, Warning,
			TEXT("%s: No attack visual settings for cue %s."),
			*GetName(), *Parameters.OriginalTag.ToString());
		return false;
	}

	const bool bAttachedSpawned = SpawnAttachedVisual(World, Mesh, Settings);
	const bool bBeamSpawned = SpawnBeam(World, Mesh, Parameters.Location, bFirstPerson, Settings);
	const bool bMuzzleSpawned = SpawnMuzzle(World, Mesh, bFirstPerson, Settings);

	return bAttachedSpawned || bBeamSpawned || bMuzzleSpawned;
}

bool UTBGameplayCue_AttackVisual::ResolveSettings(const FGameplayCueParameters& Parameters, FTBAttackVisualSettings& OutSettings) const
{
	const FTBAttackVisualSettings* Settings = AttackVisuals.FindByPredicate(
		[&Parameters](const FTBAttackVisualSettings& Candidate)
		{
			return Candidate.CueTag.IsValid() && Candidate.CueTag == Parameters.OriginalTag;
		});
	if (!Settings)
	{
		return false;
	}

	OutSettings = *Settings;
	return true;
}

bool UTBGameplayCue_AttackVisual::SpawnAttachedVisual(UWorld* World, USkeletalMeshComponent* Mesh, const FTBAttackVisualSettings& Settings) const
{
	UNiagaraSystem* System = Settings.NormalSystem.Get();

	const FName AttachSocketName = Settings.SocketName.IsNone() ? Settings.TrailEndSocketName : Settings.SocketName;
	const bool bUseSocketTrail = !Settings.SocketName.IsNone() && !Settings.TrailEndSocketName.IsNone();

	if (!IsValid(System) || !FMath::IsFinite(Settings.EffectDuration) || Settings.EffectDuration <= 0.0f
		|| (!AttachSocketName.IsNone() && !Mesh->DoesSocketExist(AttachSocketName)))
	{
		return false;
	}

	// Skeletal Mesh DI의 Filtered Sockets는 실제 소켓을 사용
	if (bUseSocketTrail && (Settings.TrailMeshParameterName.IsNone()
		|| !Mesh->GetSocketByName(Settings.SocketName) || !Mesh->GetSocketByName(Settings.TrailEndSocketName)))
	{
		UE_LOG(LogTBAttackVisual, Warning,
			TEXT("%s: Socket trail requires two sockets on %s and a Niagara Skeletal Mesh user parameter."),
			*GetName(), *GetNameSafe(Mesh));
		return false;
	}

	UNiagaraComponent* Particle =
		UNiagaraFunctionLibrary::SpawnSystemAttached(
			System, Mesh, AttachSocketName,
			Settings.LocationOffset, Settings.RotationOffset,
			EAttachLocation::KeepRelativeOffset,
			true, false, ENCPoolMethod::None, false);

	if (!IsValid(Particle))
	{
		return false;
	}

	Particle->SetRelativeScale3D(Settings.EffectScale);

	if (bUseSocketTrail)
	{
		const FString ParameterName = Settings.TrailMeshParameterName.ToString();
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
		TrailMeshInterface->SetFilteredSocketsFromBlueprints({ Settings.SocketName, Settings.TrailEndSocketName });
	}

	// 첫 프레임부터 소켓 정보와 스케일을 반영하여 재생
	Particle->Activate(true);

	FTimerHandle CleanupTimer;

	World->GetTimerManager().SetTimer(
		CleanupTimer,
		FTimerDelegate::CreateWeakLambda(
			Particle,
			[Particle]()
			{
				Particle->DestroyComponent();
			}),
		Settings.EffectDuration,
		false);

	return true;
}

bool UTBGameplayCue_AttackVisual::SpawnBeam(UWorld* World, USkeletalMeshComponent* Mesh, const FVector& TrailEnd, bool bFirstPerson, const FTBAttackVisualSettings& Settings) const
{
	UNiagaraSystem* System = Settings.TrailSystem.Get();
	if (!IsValid(System))
	{
		return false;
	}

	if (!Mesh->DoesSocketExist(Settings.MuzzleSocketName))
	{
		UE_LOG(LogTBAttackVisual, Warning,
			TEXT("%s: 총구 소켓 %s를 찾지 못했습니다."),
			*GetNameSafe(Mesh), *Settings.MuzzleSocketName.ToString());
		return false;
	}

	const FVector TrailStart = Mesh->GetSocketLocation(Settings.MuzzleSocketName);
	const FVector TrailDirection = TrailEnd - TrailStart;
	if (TrailDirection.IsNearlyZero())
	{
		return false;
	}

	UNiagaraComponent* TrailComponent = nullptr;
	if (bFirstPerson)
	{
		// 본인 화면에서는 1인칭 총구 소켓에 부착하여 이동 추적
		TrailComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
			System, Mesh, Settings.MuzzleSocketName, FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget, true, false, ENCPoolMethod::None, false);
	}
	else
	{
		// 다른 플레이어 화면에서는 발사 시점의 월드 위치 유지
		TrailComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World, System, TrailStart, TrailDirection.Rotation(), FVector::OneVector,
			true, false, ENCPoolMethod::None, false);
	}

	if (!IsValid(TrailComponent))
	{
		return false;
	}

	// 끝점은 발사 당시 서버 판정 위치로 고정
	TrailComponent->SetVariablePosition(TEXT("User.TrailEnd"), TrailEnd);
	TrailComponent->Activate(true);
	return true;
}

bool UTBGameplayCue_AttackVisual::SpawnMuzzle(UWorld* World, USkeletalMeshComponent* Mesh, bool bFirstPerson, const FTBAttackVisualSettings& Settings) const
{
	UNiagaraSystem* System = Settings.MuzzleImpactSystem.Get();
	const float Duration = bFirstPerson ? Settings.FirstPersonMuzzleImpactDuration : Settings.MuzzleImpactDuration;
	if (!IsValid(System) || !Mesh->DoesSocketExist(Settings.MuzzleSocketName)
		|| !FMath::IsFinite(Duration) || Duration <= 0.0f)
	{
		return false;
	}

	UNiagaraComponent* MuzzleComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
		System, Mesh, Settings.MuzzleSocketName, FVector::ZeroVector, FRotator::ZeroRotator,
		EAttachLocation::SnapToTarget, true, false, ENCPoolMethod::None, false);
	if (!IsValid(MuzzleComponent))
	{
		return false;
	}

	MuzzleComponent->SetRelativeScale3D(Settings.MuzzleImpactScale);
	MuzzleComponent->Activate(true);

	// 방출 종료 후 남은 파티클의 수명이 끝나면 자동 정리
	const TWeakObjectPtr<UNiagaraComponent> WeakMuzzleComponent(MuzzleComponent);
	FTimerHandle MuzzleTimer;
	World->GetTimerManager().SetTimer(MuzzleTimer, [WeakMuzzleComponent]()
	{
		if (UNiagaraComponent* Component = WeakMuzzleComponent.Get())
		{
			Component->Deactivate();
		}
	}, Duration, false);

	return true;
}
