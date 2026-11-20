#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "TBAnimNotify_MonsterMelee.generated.h"

// Existing monster montages forward their hit window to the shared melee trace.
UCLASS(meta = (DisplayName = "Monster Melee Hit Window"))
class DEVELROCKET_TEAMPROJ_API UTBAnimNotifyState_MonsterMelee : public UAnimNotifyState
{
	GENERATED_BODY()
public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};

// Existing montages also store the beginning and end as separate notifies.
UCLASS(meta = (DisplayName = "Monster Melee Hit Start"))
class UTBAnimNotify_MonsterMeleeStart : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};

UCLASS(meta = (DisplayName = "Monster Melee Hit End"))
class UTBAnimNotify_MonsterMeleeEnd : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};

