#pragma once

#include "Animation/AnimNotifies/AnimNotify.h"
#include "TBAnimNotify_HunterSwordDash.generated.h"

UCLASS(meta = (DisplayName = "Hunter Sword Dash"))
class DEVELROCKET_TEAMPROJ_API UTBAnimNotify_HunterSwordDash : public UAnimNotify
{
	GENERATED_BODY()

public:
	UTBAnimNotify_HunterSwordDash();
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
