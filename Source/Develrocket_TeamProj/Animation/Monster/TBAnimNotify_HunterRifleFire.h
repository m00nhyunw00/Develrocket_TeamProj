#pragma once

#include "Animation/AnimNotifies/AnimNotify.h"
#include "TBAnimNotify_HunterRifleFire.generated.h"

UCLASS(meta = (DisplayName = "Hunter Rifle Fire"))
class DEVELROCKET_TEAMPROJ_API UTBAnimNotify_HunterRifleFire : public UAnimNotify
{
	GENERATED_BODY()

public:
	UTBAnimNotify_HunterRifleFire();
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
