#include "AbilitySystem/GameplayCues/TBGameplayCue_AttachedLoop.h"

#include "Character/Player/TBPlayer.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

ATBGameplayCue_AttachedLoop::ATBGameplayCue_AttachedLoop(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    // 큐 제거 직후 GAS가 큐 액터를 회수하도록 설정
    bAutoDestroyOnRemove = true;
    AutoDestroyDelay = 0.0f;
}

bool ATBGameplayCue_AttachedLoop::WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
    Super::WhileActive_Implementation(MyTarget, Parameters);

    // 자기 강화와 아군에게 받은 강화 모두 효과를 받은 캐릭터에 표시
    ACharacter* Character = Cast<ACharacter>(MyTarget);
    if (!IsValid(Character) || GetNetMode() == NM_DedicatedServer || !IsValid(LoopSystem))
    {
        return false;
    }

    // 같은 큐에 활성 이벤트가 다시 들어왔을 때 중복 생성 방지
    if (IsValid(ActiveEffect))
    {
        return true;
    }

    // 원격 플레이어와 몬스터는 전신 메시, 본인 플레이어는 1인칭 팔 메시
    USkeletalMeshComponent* Mesh = Character->GetMesh();
    const ATBPlayer* Player = Cast<ATBPlayer>(Character);
    if (Player && Player->IsPlayerControlled() && Player->IsLocallyControlled())
    {
        Mesh = Player->GetFirstPersonMesh();
    }

    if (!IsValid(Mesh) || (!SocketName.IsNone() && !Mesh->DoesSocketExist(SocketName)))
    {
        return false;
    }

    ActiveEffect = UNiagaraFunctionLibrary::SpawnSystemAttached(
        LoopSystem.Get(), Mesh, SocketName,
        LocationOffset, RotationOffset, EffectScale,
        EAttachLocation::KeepRelativeOffset,
        /*bAutoDestroy=*/false, ENCPoolMethod::None,
        /*bAutoActivate=*/true, /*bPreCullCheck=*/false);

    return IsValid(ActiveEffect);
}

bool ATBGameplayCue_AttachedLoop::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
    StopLoopEffect();
    Super::OnRemove_Implementation(MyTarget, Parameters);
    return true;
}

void ATBGameplayCue_AttachedLoop::StopLoopEffect()
{
    if (IsValid(ActiveEffect))
    {
        ActiveEffect->DestroyComponent();
    }

    ActiveEffect = nullptr;
}

bool ATBGameplayCue_AttachedLoop::Recycle()
{
    StopLoopEffect();
    return Super::Recycle();
}

void ATBGameplayCue_AttachedLoop::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopLoopEffect();
    Super::EndPlay(EndPlayReason);
}