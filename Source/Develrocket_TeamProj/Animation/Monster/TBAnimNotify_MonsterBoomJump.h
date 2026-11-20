#pragma once

#include "Animation/AnimNotifies/AnimNotify.h"
#include "TBAnimNotify_MonsterBoomJump.generated.h"

UCLASS(meta = (DisplayName = "Monster Boom Jump"))
class DEVELROCKET_TEAMPROJ_API UTBAnimNotify_MonsterBoomJump : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
