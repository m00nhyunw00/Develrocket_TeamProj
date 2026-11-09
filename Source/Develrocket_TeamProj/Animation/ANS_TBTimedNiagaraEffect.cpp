#include "Animation/ANS_TBTimedNiagaraEffect.h"

#include "Character/Player/TBPlayer.h"
#include "Components/SkeletalMeshComponent.h"

UFXSystemComponent* UANS_TBTimedNiagaraEffect::SpawnEffect(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) const
{
	if (!IsValid(MeshComp))
	{
		return nullptr;
	}

	const ATBPlayer* Player = Cast<ATBPlayer>(MeshComp->GetOwner());

	// 로컬 플레이어의 전신 메시에서 파티클 생성 제외
	if (Player && Player->IsLocallyControlled() && MeshComp == Player->GetMesh())
	{
		return nullptr;
	}

	// 나머지 메시에서는 기존 Niagara 생성 처리
	return Super::SpawnEffect(MeshComp, Animation);
}