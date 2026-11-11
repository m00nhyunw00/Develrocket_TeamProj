#pragma once

#include "Animation/AnimNotifies/AnimNotify.h"
#include "TBAnimNotify_MonsterLeap.generated.h"

// 도약 몽타주의 이륙 프레임에 배치하는 Notify
UCLASS(meta = (DisplayName = "Monster Leap Launch"))
class DEVELROCKET_TEAMPROJ_API UTBAnimNotify_MonsterLeap : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
