#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "TBAnimNotify_MonsterMelee.generated.h"

UCLASS(meta = (DisplayName = "Monster Melee Hit Start"))
class DEVELROCKET_TEAMPROJ_API UTBAnimNotify_MonsterMeleeStart : public UAnimNotify
{
	GENERATED_BODY()
public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};

UCLASS(meta = (DisplayName = "Monster Melee Hit End"))
class DEVELROCKET_TEAMPROJ_API UTBAnimNotify_MonsterMeleeEnd : public UAnimNotify
{
	GENERATED_BODY()
public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
