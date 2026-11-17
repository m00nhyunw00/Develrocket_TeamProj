#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "TBAnimNotify_MonsterMelee.generated.h"

// A single montage interval controls the existing server-side melee hit window.
UCLASS(meta = (DisplayName = "Monster Melee Hit Window"))
class DEVELROCKET_TEAMPROJ_API UTBAnimNotifyState_MonsterMelee : public UAnimNotifyState
{
	GENERATED_BODY()
public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};

