#pragma once

#include "CoreMinimal.h"
#include "AnimNotifyState_TimedNiagaraEffect.h"
#include "ANS_TBTimedNiagaraEffect.generated.h"

/**
 * 
 */
UCLASS(meta = (DisplayName = "TB Timed Niagara Effect"))
class DEVELROCKET_TEAMPROJ_API UANS_TBTimedNiagaraEffect : public UAnimNotifyState_TimedNiagaraEffect
{
	GENERATED_BODY()

protected:
	virtual UFXSystemComponent* SpawnEffect(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) const override;
};